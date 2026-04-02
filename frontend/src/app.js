import { auth, provider, signInWithPopup, createSession, pushDataBucket, finishSession } from './firebase-config.js';

let currentSessionId = null;
let currentUserId = null;
let currentEegBucket = [];
let currentHrBucket = [];
let uploadInterval = null;

// Initialize Chart.js
let bioChart = null;
const ctx = document.getElementById('bioChart');
if (ctx) {
    bioChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                { 
                  label: 'BPM', 
                  data: [], 
                  borderColor: '#e71d36', 
                  fill: false,
                  borderWidth: 2,
                  pointRadius: 0,
                  tension: 0.1
                },
                { 
                  label: 'Alpha (x100)', 
                  data: [], 
                  borderColor: '#2ec4b6', 
                  fill: false,
                  borderWidth: 2,
                  pointRadius: 0,
                  tension: 0.1
                },
                { 
                  label: 'Beta (x100)', 
                  data: [], 
                  borderColor: '#FDFFFC', 
                  fill: false,
                  borderWidth: 1,
                  borderDash: [5, 5],
                  pointRadius: 0,
                  tension: 0.1
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 0 },
            interaction: { mode: 'nearest', intersect: false },
            scales: {
                x: { display: false, grid: { display: false } },
                y: { 
                    display: true, 
                    position: 'right',
                    grid: { color: 'rgba(253,255,252,0.05)' },
                    ticks: { color: '#8492A6', font: { family: "'Space Mono', monospace", size: 10 } }
                }
            },
            plugins: {
                legend: { 
                    position: 'top',
                    align: 'end',
                    labels: { color: '#FDFFFC', font: { family: "'Space Mono', monospace", size: 11 }, usePointStyle: true, boxWidth: 6 } 
                }
            }
        }
    });
}

// UI Elements
const loginScreen = document.getElementById('login-screen');
const dashboardScreen = document.getElementById('dashboard-screen');
const endScreen = document.getElementById('end-screen');
const btnLogin = document.getElementById('btn-google-login');
const btnEnd = document.getElementById('btn-end-session');
const btnRestart = document.getElementById('btn-restart');

const sphere = document.getElementById('aura-sphere');
const valBpm = document.getElementById('val-bpm');
const valAlpha = document.getElementById('val-alpha');
const valBeta = document.getElementById('val-beta');
const valState = document.getElementById('val-state');

// UI logic below

// WebSockets will now be exclusively handled by admin.js.

// Transition logic
function showDashboard() {
    loginScreen.classList.remove('active');
    loginScreen.classList.add('hidden');
    setTimeout(() => {
        dashboardScreen.classList.remove('hidden');
        dashboardScreen.classList.add('active');
    }, 700);
}

btnLogin.addEventListener('click', async () => {
    try {
        const result = await signInWithPopup(auth, provider);
        currentUserId = result.user.uid;
        currentSessionId = await createSession(currentUserId);
    } catch (e) {
        currentUserId = "demo_user";
        currentSessionId = "demo_session_" + Date.now();
    }
    showDashboard();
    uploadInterval = setInterval(async () => {
        if(currentEegBucket.length > 0 || currentHrBucket.length > 0) {
            try { await pushDataBucket(currentUserId, currentSessionId, currentEegBucket, currentHrBucket); } catch(e) { }
            currentEegBucket = []; currentHrBucket = [];
        }
    }, 2000); 
});

btnEnd.addEventListener('click', async () => {
    // 1. Transición visual INMEDIATA (antes de que Firebase pueda trabar el código)
    dashboardScreen.classList.remove('active');
    dashboardScreen.classList.add('hidden');
    setTimeout(() => {
        endScreen.classList.remove('hidden');
        endScreen.classList.add('active');
    }, 700);

    // 2. Congelar subidas y escuchas
    clearInterval(uploadInterval);
    if(mirrorWs) { mirrorWs.close(); mirrorWs = null; }
    
    // 3. Subir el último remanente
    try { 
        if (currentEegBucket.length > 0 || currentHrBucket.length > 0) {
            await pushDataBucket(currentUserId, currentSessionId, currentEegBucket, currentHrBucket); 
        }
        await finishSession(currentUserId, currentSessionId); 
    } catch(e) { }
});

if (btnRestart) {
    btnRestart.addEventListener('click', () => {
        window.location.reload(); 
    });
}

// Endpoint global inyectado por Unreal Engine WebBrowser C++
window.updateBioData = (bpm, alpha, beta) => {
    valBpm.innerText = Math.round(bpm);
    valAlpha.innerText = alpha.toFixed(3);
    valBeta.innerText = beta.toFixed(3);
    
    const timestamp = Date.now();
    currentHrBucket.push({ timestamp, bpm });
    currentEegBucket.push({ timestamp, alpha, beta });
    
    // Update Chart
    if (bioChart) {
        const timeLabel = new Date().toLocaleTimeString();
        bioChart.data.labels.push(timeLabel);
        bioChart.data.datasets[0].data.push(bpm);
        bioChart.data.datasets[1].data.push(alpha * 100);
        bioChart.data.datasets[2].data.push(beta * 100);
        
        // Mantener un historial mucho más largo (200 puntos) para que la gráfica avance lentamente
        if (bioChart.data.labels.length > 200) {
            bioChart.data.labels.shift();
            bioChart.data.datasets.forEach(dataset => dataset.data.shift());
        }
        bioChart.update();
    }
    
    updateVisuals(bpm, alpha, beta);
};

// Logica sensorial
function updateVisuals(bpm, alpha, beta) {
    if (alpha > beta * 1.5 && bpm < 80) {
        valState.innerText = "CALMA PROFUNDA";
        sphere.style.background = "var(--aura-calm)";
        sphere.style.transform = "scale(1.3)";
        sphere.style.filter = "blur(70px)";
        sphere.style.animationDuration = "6s";
    } else if (beta > alpha || bpm >= 100) {
        valState.innerText = "ALTA ACTIVIDAD";
        sphere.style.background = "var(--aura-stress)";
        sphere.style.transform = "scale(0.85)";
        sphere.style.filter = "blur(25px)";
        sphere.style.animationDuration = "1.5s";
    } else {
        valState.innerText = "SINTONIZANDO";
        sphere.style.background = "var(--aura-neutral)";
        sphere.style.transform = "scale(1)";
        sphere.style.filter = "blur(50px)";
        sphere.style.animationDuration = "4s";
    }
}

// Simulador ahora reside exclusivamente en admin.js

// ========= MODO ESPEJO (WEB PREVIEW) =========
// Permite que la web reciba datos directamente de Node para testeos sin Unreal.
let mirrorWs = new WebSocket('ws://localhost:8080');
mirrorWs.onmessage = (event) => {
    try {
        const msg = JSON.parse(event.data);
        if (msg.type === 'mirror_bio') {
            window.updateBioData(msg.bpm, msg.alpha, msg.beta);
        }
    } catch(e){}
};
mirrorWs.onerror = () => {};
