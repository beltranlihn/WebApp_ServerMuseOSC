import { MuseClient } from 'https://cdn.skypack.dev/muse-js';

const oscStatus = document.getElementById('osc-status');
const oscLog = document.getElementById('osc-log');
const inpOscIp = document.getElementById('inp-osc-ip');
const inpOscPort = document.getElementById('inp-osc-port');
const btnUpdateConn = document.getElementById('btn-update-conn');
const btnConnectMuse = document.getElementById('btn-connect-muse');

let ws = null;
let museClient = null;
let simInterval = null;
let simTick = 0;

// ==========================================
// TRUE RESONANCE (CALM SCORE) STATE MACHINE
// ==========================================
let appState = 'INITIALIZING'; // INITIALIZING, CONNECTING, CALIBRATING, RUNNING
let calibrationData = [];
let baselinePromedio = 1;
let baselineStdDev = 0.001;
let smoothedCalmScore = 0;
let calibrationProgress = 0; // (0 to 100)
const MA_WINDOW = 180; // Ventana de media móvil (3s a 60Hz)
let lastNCampsScores = [];
let dynamicAlphaEma = 0.05;
let sessionTotalTicks = 0;
let sessionCalmTicks = 0;
let calmFinal = 0.0;

const TARGET_CALIBRATION_SAMPLES = 1800; // 30s de datos (a 60Hz)

// Referencias UI
const indState = document.getElementById('indicator-state');
const indDesc = document.getElementById('indicator-desc');
const progContainer = document.getElementById('progress-bar-container');
const progBar = document.getElementById('progress-bar');
const btnSimulate = document.getElementById('btn-simulate'); // If doesn't exist, will be null

// Variables de lectura cruda por ventana
let rawAlpha = 0.5;
let rawBeta = 0.5;
let rawGamma = 0.1;
let rawTheta = 0.5;
let rawDelta = 0.5;

// Variables de hardware adicionales
let currentGyro = [0, 0, 0];
let currentAccel = [0, 0, 0];
let sensorOn = 0;
let sensorActive = 0;
let eegWatchdog = null;

// Mantenemos BPM separado simulado o extraído (Muse 2 PPG no activo por defecto)
let currentBpm = 75;

function smooth(current, target, factor = 0.05) {
    if (current === undefined || Number.isNaN(current)) return target;
    return current + (target - current) * factor;
}

function logOSC(msg) {
    const entry = document.createElement('div');
    entry.innerText = msg;
    oscLog.appendChild(entry);
    if (oscLog.children.length > 30) oscLog.removeChild(oscLog.firstChild);
    oscLog.scrollTop = oscLog.scrollHeight;
}

function initOSCRelay() {
    ws = new WebSocket('ws://localhost:8080');
    ws.onopen = () => {
        oscStatus.innerText = "ONLINE";
        oscStatus.className = "status-indicator online";
        logOSC("¡Conectado al servidor de Relay WS!");
    };
    ws.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        if (msg.type === 'config_sync') {
            inpOscIp.value = msg.ip;
            inpOscPort.value = msg.port;
            logOSC(`Config. actual Unreal: ${msg.ip}:${msg.port}`);
        }
        else if(msg.status === 'OSC_SENT') {
            // Uncoment if you want to see all packets, but it floods the console
            // logOSC(`>> Datagrama UDP OK -> [${msg.values.map(v => Number(v).toFixed(2)).join(', ')}]`);
        }
        else if(msg.status === 'info') {
            logOSC(`[Server Info] ${msg.msg}`);
        }
    };
    ws.onclose = () => {
        if (oscStatus.innerText !== "OFFLINE") {
            oscStatus.innerText = "OFFLINE";
            oscStatus.className = "status-indicator offline";
            logOSC("Error/Desconectado. Reintentando en 5s...");
        }
        setTimeout(initOSCRelay, 5000);
    };
    ws.onerror = () => { };
}
initOSCRelay();

btnUpdateConn.addEventListener('click', () => {
    if (ws && ws.readyState === WebSocket.OPEN) {
        logOSC(`Cambiando IP/Puerto a ${inpOscIp.value}:${inpOscPort.value}...`);
        ws.send(JSON.stringify({
            type: 'config_update',
            ip: inpOscIp.value,
            port: parseInt(inpOscPort.value)
        }));
    } else {
        logOSC("FALLO: El servidor WS no está corriendo.");
    }
});

function setAppState(state, desc) {
    appState = state;
    if(indState) indState.innerText = state;
    if(indDesc) indDesc.innerText = desc;
}

function broadcastFullTelemetry(calmScore) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        
        let outBpm = currentBpm;
        let outA = rawAlpha, outB = rawBeta, outG = rawGamma, outT = rawTheta, outD = rawDelta;
        let outCalm = calmScore;
        let outGyro = currentGyro;
        let outAccel = currentAccel;
        let outCalib = calibrationProgress;
        let outCalmFinal = calmFinal;
        
        // Kill-switch coercitivo
        if (sensorOn === 0 || sensorActive === 0) {
            outBpm = 0;
            outA = 0; outB = 0; outG = 0; outT = 0; outD = 0;
            outCalm = 0;
            outGyro = [0, 0, 0];
            outAccel = [0, 0, 0];
            outCalib = 0;
            outCalmFinal = 0;
        }

        ws.send(JSON.stringify({ 
            type: 'full_telemetry', 
            sensorOn: sensorOn,
            sensorActive: sensorActive,
            gyro: outGyro,
            accel: outAccel,
            alpha: outA,
            beta: outB,
            gamma: outG,
            theta: outT,
            delta: outD,
            calm: outCalm,
            bpm: outBpm,
            calibProgress: outCalib,
            calm_final: outCalmFinal
        }));
    }
}

btnConnectMuse.addEventListener('click', async () => {
    try {
        if (!navigator.bluetooth) {
            alert("⚠️ Web Bluetooth no soportado."); return;
        }

        setAppState('CONNECTING', 'Emparejando casco Bluetooth...');
        btnConnectMuse.innerText = "CONECTANDO...";
        if (!museClient) { museClient = new MuseClient(); }
        await museClient.connect();
        await museClient.start();
        
        btnConnectMuse.innerText = "MUSE CONECTADO";
        btnConnectMuse.disabled = true;
        if(btnSimulate) btnSimulate.disabled = true;
        
        sensorOn = 1; // Bandera de driver conectada exitosamente

        // Listener activo para desconexiones físicas del hardware (Botón off, Batería, etc.)
        museClient.connectionStatus.subscribe(status => {
            sensorOn = status ? 1 : 0;
            if (!status) {
                sensorActive = 0;
                setAppState('DISCONNECTED', 'Enlace Bluetooth de diadema totalmente desconectado.');
                btnConnectMuse.innerText = "RECONECTAR MUSE";
                btnConnectMuse.disabled = false;
            }
        });

        setAppState('CALIBRATING', 'Cierra los ojos y no aprietes los dientes...');
        if(progContainer) progContainer.style.display = "block";

        // Bucle Principal de Agente Legítimo (10Hz)
        // Separa la lógica de la saturación de 256Hz del hardware
        setInterval(() => {
            // 1. Ratio Base actual integrando Gamma como peso cognitivo
            // Simulamos haber extraido suma de potencias FFT
            let currentRatio = rawAlpha / (rawBeta + (0.4 * rawGamma) + 0.001); 

            // 2. Detección de Artefactos Musculares (EMG)
            let isArtifact = rawGamma > 0.8; // Umbral de pico de mandíbula
            if (isArtifact) {
                currentRatio = 0.0;
            }

            if (appState === 'CALIBRATING') {
                if (sensorActive === 0) {
                    setAppState('CALIBRATING', 'Casco inactivo. Ajústalo a tu frente...');
                    broadcastFullTelemetry(0.0);
                    return; // Congelamos el cronómetro de calibración hasta que vuelva la lectura
                }

                // Siempre empujamos para evitar que la UI se trabe por ruido estático
                calibrationData.push(currentRatio);
                calibrationProgress = (calibrationData.length / TARGET_CALIBRATION_SAMPLES) * 100;
                if(progBar) progBar.style.width = `${Math.min(calibrationProgress, 100)}%`;
                
                if (isArtifact) {
                    setAppState('CALIBRATING', '¡Ruido detectado! (Tensión muscular)');
                } else {
                    setAppState('CALIBRATING', 'Cierra los ojos y no aprietes los dientes...');
                }

                if (calibrationData.length >= TARGET_CALIBRATION_SAMPLES) {
                    baselinePromedio = calibrationData.reduce((a, b) => a + b, 0) / calibrationData.length;
                    
                    let variance = calibrationData.reduce((a, b) => a + Math.pow(b - baselinePromedio, 2), 0) / calibrationData.length;
                    baselineStdDev = Math.sqrt(variance);
                    if (baselineStdDev < 0.0001) baselineStdDev = 0.0001; // Proteccion Div0

                    setAppState('RUNNING', `Base: ${baselinePromedio.toFixed(2)} | SD: ${baselineStdDev.toFixed(2)}`);
                    if(progContainer) progContainer.style.display = "none";
                }

                // Inyectamos transmisión constate durante la calibración
                // Para que el Node Relay envíe el 'mirror_bio' con el calibProgress actualizado
                broadcastFullTelemetry(0.0);
            } else if (appState === 'RUNNING') {
                calibrationProgress = 100.0;
                // Restauramos descripción limpia si venía de un artefacto
                if(indState && indState.innerText === 'RUNNING') setAppState('RUNNING', 'Emisión OSC de sesion iniciada');
                
                // Mapeo por Z-Score
                let zScore = (currentRatio - baselinePromedio) / baselineStdDev;
                zScore = Math.min(Math.max(zScore, -2.0), 2.0); // Clamp [-2, 2]
                let normalizedScore = (zScore + 2.0) / 4.0; // Mapeo lineal a [0.0, 1.0]

                // Media Móvil
                lastNCampsScores.push(normalizedScore);
                if (lastNCampsScores.length > MA_WINDOW) lastNCampsScores.shift();
                
                smoothedCalmScore = lastNCampsScores.reduce((a, b) => a + b, 0) / lastNCampsScores.length;

                // smoothedCalmScore ya está en un rango perfecto [0.0 - 1.0] gracias al clamping del zScore previo
                let finalOSCValue = smoothedCalmScore;

                if (isArtifact) {
                    finalOSCValue = 0.0; // Rechazo absoluto inmediato
                    setAppState('RUNNING', 'Tensión muscular detectada (Calma a 0)');
                }

                // Lógica final de sesión (% de tiempo en calma sobre 0.7)
                if (sensorActive === 1) {
                    sessionTotalTicks++;
                    if (finalOSCValue > 0.7) sessionCalmTicks++;
                    calmFinal = sessionCalmTicks / sessionTotalTicks;
                }

                broadcastFullTelemetry(finalOSCValue);
            }
        }, 16); 

        // Escucha cruda de Bluetooth y parseado simulado (Alta frecuencia)
        museClient.eegReadings.subscribe(reading => {
            clearTimeout(eegWatchdog);
            sensorActive = 1;

            const samples = reading.samples;
            if (!samples || samples.length === 0) return;
            const avgPower = samples.reduce((acc, val) => acc + Math.abs(val), 0) / samples.length;
            
            // Detectamos si el casco está puesto en la frente (Skin Contact).
            // Si lo dejan en la mesa genera un 'flatline' absoluto muy bajo (< 1.0uV).
            if (avgPower < 1.0) {
                sensorActive = 0;
            } else {
                sensorActive = 1;
            }
            
            rawAlpha = smooth(rawAlpha, ((avgPower * 0.1) % 1.0), dynamicAlphaEma); 
            rawBeta  = smooth(rawBeta,  ((avgPower * 0.2) % 1.0), 0.05);
            rawTheta = smooth(rawTheta, ((avgPower * 0.15) % 1.0), 0.05);
            rawDelta = smooth(rawDelta, ((avgPower * 0.05) % 1.0), 0.05);
            rawGamma = (avgPower > 400) ? 1.0 : smooth(rawGamma, 0.1, 0.05);
            currentBpm = smooth(currentBpm, 70 + (avgPower % 30), 0.01); 

            // Watchdog para detectar desconexión (si deja de enviar datos la antena cae a sensorActive=0)
            eegWatchdog = setTimeout(() => { sensorActive = 0; }, 500);
        });

        // Suscripciones de Movimiento (Hardware I2C MPU real del Muse)
        museClient.gyroscopeData.subscribe(reading => {
            if (reading.samples.length > 0) {
                const s = reading.samples[reading.samples.length - 1];
                currentGyro[0] = smooth(currentGyro[0], s.x, 0.2);
                currentGyro[1] = smooth(currentGyro[1], s.y, 0.2);
                currentGyro[2] = smooth(currentGyro[2], s.z, 0.2);
            }
        });

        museClient.accelerometerData.subscribe(reading => {
            if (reading.samples.length > 0) {
                const s = reading.samples[reading.samples.length - 1];
                currentAccel[0] = smooth(currentAccel[0], s.x, 0.2);
                currentAccel[1] = smooth(currentAccel[1], s.y, 0.2);
                currentAccel[2] = smooth(currentAccel[2], s.z, 0.2);

                // EMA Filter Inercial dinámico para amortiguar saltos violentos
                let magnitude = Math.sqrt(s.x*s.x + s.y*s.y + s.z*s.z);
                dynamicAlphaEma = magnitude > 1.2 ? 0.01 : 0.05;
            }
        });
        
    } catch (e) {
        console.error('Error:', e);
        sensorOn = 0;
        sensorActive = 0;
        setAppState('DISCONNECTED', 'Error al conectar.');
        btnConnectMuse.innerText = "ERROR - REINTENTAR";
        btnConnectMuse.disabled = false;
    }
});
