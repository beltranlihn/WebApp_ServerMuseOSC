// Firebase fue desactivado temporalmente para priorizar Telemetría Local OSC.
// TODO: Revertir estos Mocks cuando se tenga un proyecto válido en GCP y se requiera Firestore.

export const auth = {};
export const db = {};
export const provider = {};

export async function signInWithPopup() {
    console.log("[Firebase Mock] Autenticación Simulada Exitosa");
    return { user: { uid: "vr_user_" + Date.now() } };
}

export async function signOut() {
    console.log("[Firebase Mock] Logout Simulado");
}

export async function createSession(userId) {
    const sessionId = "session_" + Date.now().toString();
    console.log(`[Firebase Mock] Sesión Creada: ${sessionId}`);
    return sessionId;
}

export async function pushDataBucket(userId, sessionId, eegArray, hrArray) {
    // Silenciado para no llenar la consola, pero aquí se inyectaría a Firestore
}

export async function finishSession(userId, sessionId) {
    console.log(`[Firebase Mock] Sesión Finalizada en la Nube: ${sessionId}`);
}
