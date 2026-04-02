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
}

initUDP();

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
                    calm, bpm, calibProgress
                } = parsedData;

                const finalStatusOn = sensorOn ? 1 : 0;
                const finalStatusActive = sensorActive ? 1 : 0;
                const isOff = (finalStatusOn === 0 || finalStatusActive === 0);

                // Enviar direcciones OSC completamente separadas para proveer "Nombres" directos (Ideal para TouchDesigner)
                const p = "/muse";
                udpPort.send({ address: `${p}/status_on`, args: [{ type: "f", value: finalStatusOn }] }); // TD prefiere Floats
                udpPort.send({ address: `${p}/status_active`, args: [{ type: "f", value: finalStatusActive }] });
                udpPort.send({ address: `${p}/gyro1`, args: [{ type: "f", value: (isOff || !gyro) ? 0.0 : gyro[0] }] });
                udpPort.send({ address: `${p}/gyro2`, args: [{ type: "f", value: (isOff || !gyro) ? 0.0 : gyro[1] }] });
                udpPort.send({ address: `${p}/gyro3`, args: [{ type: "f", value: (isOff || !gyro) ? 0.0 : gyro[2] }] });
                udpPort.send({ address: `${p}/accel1`, args: [{ type: "f", value: (isOff || !accel) ? 0.0 : accel[0] }] });
                udpPort.send({ address: `${p}/accel2`, args: [{ type: "f", value: (isOff || !accel) ? 0.0 : accel[1] }] });
                udpPort.send({ address: `${p}/accel3`, args: [{ type: "f", value: (isOff || !accel) ? 0.0 : accel[2] }] });
                udpPort.send({ address: `${p}/eeg_alpha`, args: [{ type: "f", value: isOff ? 0.0 : alpha }] });
                udpPort.send({ address: `${p}/eeg_beta`, args: [{ type: "f", value: isOff ? 0.0 : beta }] });
                udpPort.send({ address: `${p}/eeg_gamma`, args: [{ type: "f", value: isOff ? 0.0 : gamma }] });
                udpPort.send({ address: `${p}/eeg_theta`, args: [{ type: "f", value: isOff ? 0.0 : theta }] });
                udpPort.send({ address: `${p}/eeg_delta`, args: [{ type: "f", value: isOff ? 0.0 : delta }] });
                udpPort.send({ address: `${p}/calm_state`, args: [{ type: "f", value: isOff ? 0.0 : calm }] });
                // JSON.stringify convierte NaN en null, así que la validación debe atrapar nulos y strings malos
                let safeBPM = (bpm == null || isNaN(bpm)) ? 75.0 : Number(bpm);
                udpPort.send({ address: `${p}/heart_rate`, args: [{ type: "f", value: isOff ? 0.0 : safeBPM }] });
                udpPort.send({ address: `${p}/calib_progress`, args: [{ type: "f", value: isOff ? 0.0 : (calibProgress || 0.0) }] });

                // Mantenemos soporte del Modo Espejo UI legacy enviando el mirror_bio
                const mirrorMsg = JSON.stringify({ type: 'mirror_bio', bpm: bpm, alpha: alpha, beta: beta });
                wss.clients.forEach(client => {
                    if (client.readyState === WebSocket.OPEN) client.send(mirrorMsg);
                });

                if (Math.random() < 0.05) { // Console throttling
                    ws.send(JSON.stringify({ status: 'info', msg: `Matriz Emitida: Act [${sensorActive}], Calm [${calm.toFixed(2)}]` }));
                }
            }
        } catch (e) {
            console.error("[Relay] Error de parseo o envío: ", e);
        }
    });
});

console.log("[Relay] Servidor corriendo. WS en localhost:8080");
