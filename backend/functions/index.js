const functions = require("firebase-functions");
const admin = require("firebase-admin");

admin.initializeApp();
const db = admin.firestore();

exports.analyzeSession = functions.firestore
    .document('sessions/{sessionId}')
    .onUpdate(async (change, context) => {
        const newData = change.after.data();
        const previousData = change.before.data();

        // Ensure session is newly completed
        if (newData.status === 'completed' && previousData.status !== 'completed') {
            const userId = newData.userId;
            const eegData = newData.eegBucket || []; 
            const hrData = newData.hrBucket || [];

            if (eegData.length === 0 || hrData.length === 0) {
                console.log("No biometric data in this session.");
                return null;
            }

            // Analytical calculations
            const maxHr = Math.max(...hrData.map(d => d.bpm));
            const avgHr = hrData.reduce((acc, d) => acc + d.bpm, 0) / hrData.length;
            
            const avgAlpha = eegData.reduce((acc, d) => acc + d.alpha, 0) / eegData.length;
            const avgBeta = eegData.reduce((acc, d) => acc + d.beta, 0) / eegData.length;
            
            const calmMoments = eegData.filter(d => d.alpha > d.beta).length;

            try {
                // Fetch user data for email
                const userRecord = await admin.auth().getUser(userId);
                const userEmail = userRecord.email;

                // Setup Trigger Email extension document
                const htmlContent = `
                    <div style="font-family: Arial, sans-serif; color: #333; padding: 20px; border: 1px solid #eaeaea; border-radius: 8px;">
                        <h2 style="color: #4CAF50;">Reporte de Sesión Biorreactiva: Soul Charger</h2>
                        <p>Hola, tu última inmersión VR en Soul Charger fue analizada con éxito. Aquí están tus bio-métricas:</p>
                        <ul>
                            <li><strong>Pico de Ritmo Cardíaco:</strong> ${maxHr} BPM</li>
                            <li><strong>Promedio de Ritmo Cardíaco:</strong> ${Math.round(avgHr)} BPM</li>
                            <li><strong>Promedio Onda Alpha (Calma):</strong> ${avgAlpha.toFixed(3)} V</li>
                            <li><strong>Promedio Onda Beta (Actividad):</strong> ${avgBeta.toFixed(3)} V</li>
                            <li><strong>Lecturas predominantes de Calma:</strong> ${calmMoments}</li>
                        </ul>
                        <p>Los picos identificados han sido almacenados para ajustar la intensidad de tu próxima sesión.</p>
                        <p>Saludos,<br/>Equipo de Ingeniería - Soul Charger</p>
                    </div>
                `;

                // 'mail' collection triggers Firebase's "Trigger Email" Extension
                await db.collection('mail').add({
                    to: userEmail,
                    message: {
                        subject: 'Análisis de tu Sesión - Soul Charger (EEG/HR)',
                        html: htmlContent
                    }
                });
            } catch (err) {
                console.error("Error fetching user email or triggering email", err);
            }

            // Update original session with analytics
            return change.after.ref.update({
                analysis: { maxHr, avgHr, avgAlpha, avgBeta, calmMoments },
                analyzedAt: admin.firestore.FieldValue.serverTimestamp()
            });
        }
        return null;
    });
