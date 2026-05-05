const osc = require("osc");
const WebSocket = require("ws");

let udpPort;
let targetIp = "127.0.0.1";
let targetPort = 8000;

function initUDP() {
    if (udpPort) {
        try { udpPort.close(); } catch(e){}
    }
    
    udpPort = new osc.UDPPort({
        localAddress: "0.0.0.0",
        localPort: 8001, // Puerto local de donde sale
        remoteAddress: targetIp,
        remotePort: targetPort,
        metadata: true
    });
    
    udpPort.open();
    
    udpPort.on("ready", function () {
        console.log(`[OSC] Conexión UDP lista. Flujo enviado a -> ${targetIp}:${targetPort}`);
    });
    
    udpPort.on("error", function (err) {
        console.error("[OSC] Error UDP: ", err);
    });

    // Escucha de comandos entrantes desde Unreal
    udpPort.on("message", function (oscMsg) {
        if (oscMsg.address === "/unreal/end_session") {
            console.log("[OSC IN] Recibida orden de fin de sesión desde Unreal.");
            if (typeof wss !== 'undefined') {
                wss.clients.forEach(client => {
                    if (client.readyState === WebSocket.OPEN) {
                        client.send(JSON.stringify({ type: 'unreal_command', command: 'end_session' }));
                    }
                });
            }
        }
    });
}

initUDP();

// ==========================================
// NaN SHIELD: Retención del Último Valor Sano
// ==========================================
let lastValidValues = {
    status_on: 0.0, status_active: 0.0,
    gyro1: 0.0, gyro2: 0.0, gyro3: 0.0,
    accel1: 0.0, accel2: 0.0, accel3: 0.0,
    eeg_alpha: 0.0, eeg_beta: 0.0, eeg_gamma: 0.0, eeg_theta: 0.0, eeg_delta: 0.0,
    calm_state: 0.0, heart_rate: 75.0, calib_progress: 0.0, calm_final: 0.0
};

function safeFloat(val, key) {
    if (val == null || typeof val === 'undefined' || Number.isNaN(Number(val))) {
        return lastValidValues[key];
    }
    lastValidValues[key] = Number(val);
    return lastValidValues[key];
}

const wss = new WebSocket.Server({ port: 8080 });

wss.on('connection', function connection(ws) {
    console.log("[WS] Frontend Biorreactivo Conectado al Relay");
    
    // Al conectar, forzamos sincronizar los parámetros actuales
    ws.send(JSON.stringify({ type: 'config_sync', ip: targetIp, port: targetPort }));

    ws.on('message', function message(data) {
        try {
            const parsedData = JSON.parse(data);
            
            if (parsedData.type === 'config_update') {
               targetIp = parsedData.ip || targetIp;
               targetPort = parseInt(parsedData.port) || targetPort;
               initUDP(); // Reinicia el socket UDP hacia la nueva IP
               ws.send(JSON.stringify({ status: 'info', msg: `Ruta OSC actualizada a ${targetIp}:${targetPort}` }));
            }

            if (parsedData.type === 'bio_update' || parsedData.type === 'calm_update') {
                // Support retro-compatibility for old UI test elements if needed
                if(parsedData.type === 'calm_update') {
                    udpPort.send({ address: "/muse/v2/calm", args: [{ type: "f", value: parsedData.score }] });
                }
            }

            if (parsedData.type === 'full_telemetry') {
                const {
                    sensorOn, sensorActive, gyro, accel, 
                    alpha, beta, gamma, theta, delta, 
                    calm, bpm, calibProgress, calm_final, calib_completed, headset_id
                } = parsedData;

                let fOn = safeFloat(sensorOn ? 1 : 0, 'status_on');
                let fAct = safeFloat(sensorActive ? 1 : 0, 'status_active');
                const isOff = (fOn === 0 || fAct === 0);

                let g1 = safeFloat(gyro ? gyro[0] : 0, 'gyro1');
                let g2 = safeFloat(gyro ? gyro[1] : 0, 'gyro2');
                let g3 = safeFloat(gyro ? gyro[2] : 0, 'gyro3');

                let a1 = safeFloat(accel ? accel[0] : 0, 'accel1');
                let a2 = safeFloat(accel ? accel[1] : 0, 'accel2');
                let a3 = safeFloat(accel ? accel[2] : 0, 'accel3');

                // Aplicar escudo de ceros matemático obligatorio si el Killswitch está accionado
                const v = {
                    1: fOn, 2: fAct,
                    3: isOff ? 0.0 : g1,
                    4: isOff ? 0.0 : g2,
                    5: isOff ? 0.0 : g3,
                    6: isOff ? 0.0 : a1,
                    7: isOff ? 0.0 : a2,
                    8: isOff ? 0.0 : a3,
                    9: isOff ? 0.0 : safeFloat(alpha, 'eeg_alpha'),
                    10: isOff ? 0.0 : safeFloat(beta, 'eeg_beta'),
                    11: isOff ? 0.0 : safeFloat(gamma, 'eeg_gamma'),
                    12: isOff ? 0.0 : safeFloat(theta, 'eeg_theta'),
                    13: isOff ? 0.0 : safeFloat(delta, 'eeg_delta'),
                    14: isOff ? 0.0 : safeFloat(calm, 'calm_state'),
                    15: isOff ? 0.0 : safeFloat(bpm, 'heart_rate'),
                    16: safeFloat(calibProgress, 'calib_progress'),
                    17: isOff ? 0.0 : safeFloat(calm_final, 'calm_final'),
                    18: isOff ? 0.0 : safeFloat(calib_completed, 'calib_completed')
                };

                // Revertido al Array Bloque Único (Monolithic Payload) para evitar problemas de Blueprint
                udpPort.send({
                    address: "/muse/data",
                    args: [
                        { type: "f", value: v[1] }, { type: "f", value: v[2] },
                        { type: "f", value: v[3] }, { type: "f", value: v[4] }, { type: "f", value: v[5] },
                        { type: "f", value: v[6] }, { type: "f", value: v[7] }, { type: "f", value: v[8] },
                        { type: "f", value: v[9] }, { type: "f", value: v[10] }, { type: "f", value: v[11] },
                        { type: "f", value: v[12] }, { type: "f", value: v[13] }, 
                        { type: "f", value: v[14] }, { type: "f", value: v[15] },
                        { type: "f", value: v[16] }, { type: "f", value: v[17] }, { type: "f", value: v[18] }
                    ]
                });

                udpPort.send({
                    address: "/muse/headset",
                    args: [
                        { type: "s", value: headset_id || "Unknown_Muse" }
                    ]
                });

                // Mantenemos soporte del Modo Espejo UI legacy enviando el mirror_bio ampliado
                const mirrorMsg = JSON.stringify({ type: 'mirror_bio', calm: v[14], bpm: v[15], calibProgress: v[16] });
                wss.clients.forEach(client => {
                    if (client.readyState === WebSocket.OPEN) client.send(mirrorMsg);
                });

                if (Math.random() < 0.05) { // Console throttling 
                    ws.send(JSON.stringify({ status: 'info', msg: `Matriz Emitida: Act [${sensorActive}], Calm [${calm.toFixed(2)}]` }));
                    console.log(`[DEBUG] Enviando a OSC -> [On:${v[1]}, Act:${v[2]}, Calm:${v[14].toFixed(2)}, BPM:${v[15].toFixed(1)}]`);
                }
            }
        } catch (e) {
            console.error("[Relay] Error de parseo o envío: ", e);
        }
    });
});

console.log("[Relay] Servidor corriendo. WS en localhost:8080");
