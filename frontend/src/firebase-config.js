import { initializeApp } from "https://www.gstatic.com/firebasejs/10.8.0/firebase-app.js";
import { getAuth, GoogleAuthProvider, signInWithPopup, signOut } from "https://www.gstatic.com/firebasejs/10.8.0/firebase-auth.js";
import { getFirestore, doc, setDoc, updateDoc, arrayUnion } from "https://www.gstatic.com/firebasejs/10.8.0/firebase-firestore.js";

// TODO: Replace with actual Firebase environment config
const firebaseConfig = {
    apiKey: "YOUR_API_KEY",
    authDomain: "YOUR_PROJECT_ID.firebaseapp.com",
    projectId: "YOUR_PROJECT_ID",
    storageBucket: "YOUR_PROJECT_ID.appspot.com",
    messagingSenderId: "SENDER_ID",
    appId: "APP_ID"
};

const app = initializeApp(firebaseConfig);
const auth = getAuth(app);
const db = getFirestore(app);
const provider = new GoogleAuthProvider();

export { auth, db, provider, signInWithPopup, signOut };

// Bucketing Logic: Write arrays to Firestore instead of single docs per second
export async function createSession(userId) {
    const sessionId = Date.now().toString();
    // Ensure the users collection exists logically by putting it under 'users'
    const userRef = doc(db, 'users', userId);
    await setDoc(userRef, { lastLogin: new Date() }, { merge: true });
    
    const sessionRef = doc(db, 'users', userId, 'sessions', sessionId);
    await setDoc(sessionRef, {
        userId,
        startTime: new Date(),
        status: 'active',
        eegBucket: [],
        hrBucket: []
    });
    return sessionId;
}

// Push local accumulators to the cloud every N seconds to save costs
export async function pushDataBucket(userId, sessionId, eegArray, hrArray) {
    const sessionRef = doc(db, 'users', userId, 'sessions', sessionId);
    // Uses arrayUnion to append locally accumulated arrays into the Firestore array
    await updateDoc(sessionRef, {
        eegBucket: arrayUnion(...eegArray),
        hrBucket: arrayUnion(...hrArray)
    });
}

// Marks session completed, which triggers the Cloud Function
export async function finishSession(userId, sessionId) {
    const sessionRef = doc(db, 'users', userId, 'sessions', sessionId);
    await updateDoc(sessionRef, {
        status: 'completed',
        endTime: new Date()
    });
}
