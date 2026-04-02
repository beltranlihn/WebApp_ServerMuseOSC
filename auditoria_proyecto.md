# Auditoría Técnica: Soul Charger Biofeedback WebApp

## 1. Resumen Ejecutivo
Soul Charger es una aplicación híbrida de biorretroalimentación que funciona como puente de alta frecuencia entre dispositivos de hardware neurotecnológico (Muse 2) y motores gráficos 3D (Unreal Engine / TouchDesigner).
* **Estado Actual:** Fase de integración madura. El "Core" (núcleo comunicativo) funciona con una arquitectura estable orientada a eventos a `60Hz`.

## 2. Arquitectura de Sistemas
El proyecto se divide en dos grandes monolitos orquestados localmente:

### Frontend (Biorretroalimentación y UI)
- **Tecnologías:** HTML5, Vanilla JavaScript, CSS3 (Glassmorphism), Chart.js.
- **Conectividad:** `Web Bluetooth API` vía la librería especializada `muse-js`. Chrome acapara la conexión GATT eximiendo al OS de administrar el dispositivo en bajo nivel.
- **Métricas:** 
  - La visualización fue optimizada al retirar el vector de rellenado (`fill`) y el suavizado de cuerdas (`tension`) en Chart.js, bajando drásticamente el consumo intensivo de VRAM.
  - El bucle principal de telemetría fue desacoplado del listener `eegReadings` originario del hardware, garantizando un frame-time ultra veloz del loop de algoritmos de 16 milisegundos (`60Hz`).

### Backend (Relay OSC/UDP)
- **Tecnologías:** NodeJS, `osc` (osc.js), `ws` (WebSockets).
- **Proceso:** Opera activamente en el puerto `8080` (oyente WebSocket frontend) y despacha UDP sin conexión sobre el puerto `8000` (oyente Unreal/TD).
- **Lógica de Ruteo:** Desempaqueta un solo JSON macizo originado en la web y lo atomiza en tiempo real en los 16 conectores formales requeridos por cualquier motor en red local (TouchDesigner/Unreal Engine).

## 3. Algoritmo "True Resonance" (Calm Score)
Para aislar el estado de "Calma", se aplica la metodología de **Índice de Relajación Normalizado por Baseline**:
* **Calibración:** Fuerza una captura de `120 muestras` limpias purificadas del sensor durante la inicialización antes de poder cruzar a fase productiva. Equivalente a un pre-cálculo rápido de `2.0 segundos` en crudo.
* **Medición Directa:** Toma un ratio estricto entre ondas *Alpha* (relajación ocular/ausencia visual) frente a *Beta* (cognición/atención).
* **Barrera Anti-Tensión:** Protecciones musculares (EMG). Si el acelerómetro detecta ruido de mordida mandibular (`> 400µV` transformado en `rawGamma`), marca ruido estático visual pero ya **no congela** irreversiblemente la recolección para tolerar setups dinámicos en cuartos oscuros.

## 4. Pipeline de Mapeo OSC (Diccionario Activo)
Unreal/TD recibe un datagrama UDP paralelo por cada bloque. Formateados obligatoriamente a Floats (`f`) directos para evitar bugs de tipo fuerte de los motores gráficos.

| Dirección | Tipo | Descripción |
| :--- | :--- | :--- |
| `/muse/status_on` | Float (0 / 1) | Dispositivo Emparejado por Bluetooth |
| `/muse/status_active` | Float (0 / 1) | Diadema puesta en la base craneal (Piel detectada) |
| `/muse/gyro1` | Float | Vectores X de movimiento giroscópico |
| `/muse/gyro2` | Float | Vectores Y de movimiento giroscópico |
| `/muse/gyro3` | Float | Vectores Z de movimiento giroscópico |
| `/muse/accel1` | Float | Aceleración linear X |
| `/muse/accel2` | Float | Aceleración linear Y |
| `/muse/accel3` | Float | Aceleración linear Z |
| `/muse/eeg_alpha` | Float | Banda Alfa |
| `/muse/eeg_beta` | Float | Banda Beta (Cognición) |
| `/muse/eeg_gamma` | Float | Falso-Gamma (Utilizado para detección EMG/Mandíbula) |
| `/muse/eeg_theta` | Float | Profundidad cognitiva |
| `/muse/eeg_delta` | Float | Onda de sueño profundo |
| `/muse/calm_state` | Float | Resultado final algorítmico derivado de Alpha/Beta |
| `/muse/heart_rate` | Float | Frecuencia cardíaca procesada |
| `/muse/calib_progress` | Float (0-100)| Barra conceptual de calibración del Baseline |

## 5. Vulnerabilidades Mitigadas (Firewalls Internos)
1. **Killswitch Coercitivo Absoluto:** Prevención matemática aplicada severamente en el servidor. Si `status_on` o `status_active` caen a 0 (usuario se quita el casco o expira el watchdog del frontend), las otras 14 métricas restantes son **satanizadas/sobreescritas mecánicamente a `0.0`** justo antes de la inyección UDP. Es literalmente imposible emitir fantasmas residuales de giroides a TouchDesigner.
2. **Crash de Nulos (NaN Handling):** El paquete de latidos es extremadamente vulnerable a divisiones entre ceros durante micro-cortes del Bluetooth porque Javascript envía los cálculos fallidos como `NaN`, los cuales se degeneran como `null` atravesando el protocolo JSON y colapsan a los escuchas OSC. Se implementó un escudo validador estricto de nulos.

## 6. Próximos Pasos (Hoja de Ruta)
* **Pase de Firebase:** Las integraciones `Firestore` quedaron estructuralmente preparadas en el código `app.js` pero fueron marginadas a favor del desarrollo agresivo OSC en la segunda mitad. Deberán reactivarse si los "Sessions" son monetizados o guardados como analítica médica a la nube.
* **Hosting Mixto:** Actualmente servidor Node y Web deben compartir locación física (`localhost`). Para sacar esto a un evento en vivo o expo, se debe considerar el alojamiento de la web (HTML/JS) en la nube y el NodeJS estrictamente atado/corriendo dentro del hardware local de renderizado (TD/Unreal).
