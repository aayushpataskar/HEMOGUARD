/* ============================================================
   HEMOGUARD — Dashboard Logic
   Live ESP32 data  ·  Simulation engine  ·  Chart.js waveforms
   ============================================================ */

(() => {
  'use strict';

  /* ---------- constants ---------- */
  const CHART_POINTS = 200;
  const POLL_INTERVAL = 400;   // ms — live mode
  const SIM_INTERVAL = 100;   // ms — simulation tick

  /* ---------- patient profiles ---------- */
  const patients = {
    'patient-1': {
      name: 'Patient 1 — John D.',
      baseECG: 2100, basePPG: 51000, basePPGAmp: 1300,
      basePTT: 230, baseTempGrad: 0.8, baseBio: 45,
      age: 58, sex: 'Male', bloodType: 'A+',
      conditions: ['Type 2 Diabetes', 'Hypertension Stage 1'],
      medications: ['Metformin 500mg', 'Lisinopril 10mg'],
      allergies: ['Penicillin'],
      admitDate: '2026-04-08', room: 'ICU-12A',
    },
    'patient-2': {
      name: 'Patient 2 — Sarah M.',
      baseECG: 1950, basePPG: 48000, basePPGAmp: 1100,
      basePTT: 210, baseTempGrad: 1.0, baseBio: 50,
      age: 34, sex: 'Female', bloodType: 'O-',
      conditions: ['Post-Op Splenectomy', 'Anemia (Hb 9.2)'],
      medications: ['Iron Supplements', 'Omeprazole 20mg'],
      allergies: ['Sulfa Drugs'],
      admitDate: '2026-04-09', room: 'ICU-14B',
    },
    'patient-3': {
      name: 'Patient 3 — Raj K.',
      baseECG: 2250, basePPG: 53000, basePPGAmp: 1500,
      basePTT: 250, baseTempGrad: 0.5, baseBio: 38,
      age: 72, sex: 'Male', bloodType: 'B+',
      conditions: ['Chronic Kidney Disease Stage 3', 'Atrial Fibrillation', 'Type 1 Diabetes'],
      medications: ['Insulin Glargine', 'Warfarin 5mg', 'Amlodipine 5mg'],
      allergies: ['Iodine Contrast'],
      admitDate: '2026-04-07', room: 'ICU-11C',
    },
  };

  /* ---------- state ---------- */
  let currentPatient = 'patient-1';
  let isLiveMode = false;
  let intervalId = null;
  let prevValues = {};

  const patientState = {
    'patient-1': { type: 'stable', simTick: 0, hiiHistory: [], lastGen: null, timeBuf: [], base: null },
    'patient-2': { type: 'warning', simTick: 0, hiiHistory: [], lastGen: null, timeBuf: [], base: null },
    'patient-3': { type: 'critical', simTick: 0, hiiHistory: [], lastGen: null, timeBuf: [], base: null },
  };

  /* per-patient chart data buffers */
  const chartBuffers = {};
  Object.keys(patients).forEach(id => {
    chartBuffers[id] = {
      ecg: new Array(CHART_POINTS).fill(0),
      ppg: new Array(CHART_POINTS).fill(0),
      miniEcg: new Array(100).fill(0),
      miniChart: null
    };
  });

  /* ---------- DOM refs ---------- */
  const $ = s => document.querySelector(s);
  const $$ = s => document.querySelectorAll(s);

  const dom = {
    patientSelect: $('#patient-select'),
    modeToggle: $('#mode-toggle'),
    modeLabel: $('#mode-label'),
    ipInput: $('#esp-ip'),

    aiClinicalCard: $('#ai-clinical-card'),
    aiConfValue: $('#ai-conf-value'),
    aiReasonsList: $('#ai-reasons-list'),
    aiSummaryBox: $('#ai-summary-box'),
    aiSysText: $('#ai-sys-text'),

    hr: $('#v-hr'),
    hrv: $('#v-hrv'),
    temp: $('#v-temp'),
    ptt: $('#v-ptt'),
    bio: $('#v-bio'),

    ecgCanvas: $('#ecg-chart'),
    ppgCanvas: $('#ppg-chart'),

    alertBanner: $('#alert-banner'),
    alertIcon: $('#alert-icon'),
    alertTitle: $('#alert-title'),
    alertDesc: $('#alert-desc'),
  };

  /* ---------- Chart.js setup ---------- */
  const chartDefaults = {
    responsive: true,
    maintainAspectRatio: false,
    animation: { duration: 0 },
    plugins: { legend: { display: false } },
    scales: {
      x: { display: false },
      y: {
        display: true,
        grid: { color: 'rgba(148,163,184,.08)', drawTicks: false },
        ticks: { display: false },
        border: { display: false },
      },
    },
    elements: {
      point: { radius: 0 },
      line: { borderWidth: 1.8, tension: 0.3 },
    },
  };

  const labels = Array.from({ length: CHART_POINTS }, (_, i) => i);

  const ecgChart = new Chart(dom.ecgCanvas, {
    type: 'line',
    data: {
      labels,
      datasets: [{
        data: chartBuffers[currentPatient].ecg,
        borderColor: '#38bdf8',
        backgroundColor: 'rgba(56,189,248,.08)',
        fill: true,
      }],
    },
    options: { ...chartDefaults },
  });

  const ppgChart = new Chart(dom.ppgCanvas, {
    type: 'line',
    data: {
      labels,
      datasets: [{
        data: chartBuffers[currentPatient].ppg,
        borderColor: '#a78bfa',
        backgroundColor: 'rgba(167,139,250,.08)',
        fill: true,
      }],
    },
    options: { ...chartDefaults },
  });
  // API CALL
  async function getAIResult(features) {
    try {
      const response = await fetch("http://127.0.0.1:8000/predict", {
        method: "POST",
        headers: {
          "Content-Type": "application/json"
        },
        body: JSON.stringify(features)
      });

      const result = await response.json();
      return result;

    } catch (error) {
      console.error("Error calling AI model:", error);
      return null;
    }
  }
  /* ---------- helpers ---------- */
  function rand(min, max) { return Math.random() * (max - min) + min; }
  function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }

  function formatTime() {
    const d = new Date();
    return d.toLocaleTimeString('en-US', { hour12: false }) + '.' +
      String(d.getMilliseconds()).padStart(3, '0');
  }

  function setTrend(el, curr, prev) {
    if (!prev || Math.abs(curr - prev) < 0.01) {
      el.textContent = '—';
      el.className = 'vital-trend flat';
    } else if (curr > prev) {
      el.textContent = '↑';
      el.className = 'vital-trend up';
    } else {
      el.textContent = '↓';
      el.className = 'vital-trend down';
    }
  }

  /* ---------- AI Analysis Mathematical Engine ---------- */
  function calcBufferSlopes(buf) {
    if (buf.length < 10) return { hr: 0, ptt: 0, bio: 0, hii: 0 };
    const first = buf[0], last = buf[buf.length - 1];
    const dtSec = (last.t - first.t) / 1000;
    if (dtSec <= 0) return { hr: 0, ptt: 0, bio: 0, hii: 0 };
    return {
      hr: (last.hr - first.hr) / dtSec,
      ptt: (last.ptt - first.ptt) / dtSec,
      bio: (last.bio - first.bio) / dtSec,
      hii: (last.hii - first.hii) / dtSec,
    };
  }

  function calcStDev(buf, key) {
    if (buf.length < 2) return 0;
    const mean = buf.reduce((s, x) => s + x[key], 0) / buf.length;
    const variance = buf.reduce((s, x) => s + Math.pow(x[key] - mean, 2), 0) / buf.length;
    return Math.sqrt(variance);
  }

  function updateEMA(newVal, prevEMA, alpha = 0.2) {
    if (prevEMA === null || prevEMA === undefined) return newVal;
    return alpha * newVal + (1 - alpha) * prevEMA;
  }

  function calcSQI(data, stdevHR) {
    let sqi = 0;
    if (data.ecg_detected) sqi += 25;
    if (data.ppg_detected) sqi += 25;
    if (data.temp_detected) sqi += 25;
    if (data.bio_detected) sqi += 25;
    if (stdevHR > 15) sqi -= 20;
    return Math.max(0, Math.min(100, Math.round(sqi - (Math.random() * 4)))); // slight variance to reflect reality
  }

  function evaluatePatientAI(pId) {
    const pData = patientState[pId].lastGen;
    const pBuf = patientState[pId].timeBuf;
    const pBase = patientState[pId].base;

    if (!pData || pBuf.length < 3) return null;

    const hr_est = pBuf[pBuf.length - 1].hr;
    const slopes = calcBufferSlopes(pBuf);
    const stdevHR = calcStDev(pBuf, 'hr');
    const sqi = calcSQI(pData, stdevHR);

    const currentHii = pData.hii;
    const lastEma = pBuf.length > 1 ? pBuf[pBuf.length - 2].emaHii : currentHii;
    pBuf[pBuf.length - 1].emaHii = updateEMA(currentHii, lastEma, 0.2);

    let reasons = [];
    let severity = "stable";
    let action = "Continue routine monitoring.";
    let trendDirection = "→ STABLE";

    /* Track temp-driven severity so HII logic cannot downgrade it */
    let tempSeverity = "stable";
    let tempAction = "";
    let tempTrend = "";

    if (sqi < 50) {
      severity = "warning";
      action = "WARNING: Signal Integrity Compromised. Check sensor connections.";
      reasons.push({ label: "High sensor noise detected", val: `SQI: ${sqi}%` });
    } else {
      if (pBase) {
        const devHR = hr_est - pBase.hr;
        const devPTT = pData.ptt - pBase.ptt;
        if (devHR > 15) reasons.push({ label: "Heart Rate elevated above baseline", val: `+${devHR.toFixed(1)} bpm` });
        if (devPTT > 25) reasons.push({ label: "PTT drift (BP alteration risk)", val: `+${devPTT.toFixed(1)} ms` });
      }
      if (slopes.bio > 0.4) reasons.push({ label: "Rapid Bioimpedance Shift", val: `+${slopes.bio.toFixed(2)} Ω/s` });

      /* --- Temp Gradient check (independent of HII) --- */
      if (pData.temp_grad > 2.0) {
        tempSeverity = "warning";
        reasons.push({ label: "Temperature Gradient elevated", val: `${pData.temp_grad.toFixed(1)}°C (>2.0)` });
        tempAction = "ELEVATED TEMPERATURE GRADIENT — Possible peripheral vasoconstriction. Assess perfusion.";
        tempTrend = "↗ TEMP ELEVATED";
      }
      if (pData.temp_grad > 3.5) {
        tempSeverity = "critical";
        tempAction = "CRITICAL TEMP GRADIENT — Severe peripheral shutdown. Immediate assessment required.";
        tempTrend = "↑ TEMP CRITICAL";
      }

      /* --- HII-based severity --- */
      if (currentHii > 0.85) {
        if (slopes.hii > 0.005) {
          severity = "critical"; action = "IMMEDIATE INTERVENTION REQUIRED. Hemodynamic collapse risk."; trendDirection = "↑ WORSENING FAST";
        } else {
          severity = "warning"; action = "Monitor closely. Patient is high risk but parameters stabilizing."; trendDirection = "→ STABLE (High Risk)";
        }
      } else if (currentHii > 0.75 || (pData.temp_grad >= 1.0 && pData.temp_grad <= 2.5)) {
        severity = "warning";
        action = (pData.temp_grad >= 1.0) ? `Temperature Gradient elevated (${pData.temp_grad.toFixed(1)}°C). Early compensatory index detected.` : "Early compensatory index detected. Increase observation.";
        trendDirection = slopes.hii > 0 ? "↗ WORSENING" : "↘ IMPROVING";

        if (pData.temp_grad >= 1.0 && pData.ptt > 500) {
          severity = "critical";
          action = "PTT INSTABILITY. Combined with elevated temperature gradient, indicating risk of decompensation.";
          trendDirection = "↑ WORSENING FAST";
        }
      } else {
        if (slopes.hii > 0.035) { severity = "warning"; action = "Sudden destabilization slope detected."; trendDirection = "↗ RAPID SHIFT"; }
      }

      /* --- Enforce temp gradient floor: temp can only UPGRADE severity, never downgrade --- */
      const sevRank = { stable: 0, warning: 1, critical: 2 };
      if (sevRank[tempSeverity] > sevRank[severity]) {
        severity = tempSeverity;
        action = tempAction;
        trendDirection = tempTrend;
      }

      if (reasons.length === 0) reasons.push({ label: "All vital markers tracking nominally.", val: "STABLE" });
    }

    const contribs = [];
    if (Math.abs(slopes.hr) > 0.3) contribs.push({ name: `ΔHR/dt (${slopes.hr > 0 ? '↑' : '↓'})`, weight: Math.min(45, Math.abs(slopes.hr * 10)) });
    if (Math.abs(slopes.ptt) > 1.0) contribs.push({ name: `ΔPTT/dt (${slopes.ptt > 0 ? '↑' : '↓'})`, weight: Math.min(35, Math.abs(slopes.ptt * 4)) });
    if (Math.abs(slopes.bio) > 0.3) contribs.push({ name: `ΔBio/dt (${slopes.bio > 0 ? '↑' : '↓'})`, weight: Math.min(30, Math.abs(slopes.bio * 10)) });
    if (pData.ppg_amp < patients[pId].basePPGAmp * 0.8) contribs.push({ name: `PPG Amplitude (↓)`, weight: 25 });
    contribs.sort((a, b) => b.weight - a.weight);
    if (contribs.length === 0) contribs.push({ name: "Stable baseline parameters", weight: 100 });

    let ttr = "N/A (Stable)";
    if (slopes.hii > 0.002 && currentHii < 0.7) {
      const seconds = (0.7 - currentHii) / slopes.hii;
      if (seconds > 0 && seconds < 3600) {
        ttr = `${Math.floor(seconds / 60)}m ${Math.floor(seconds % 60)}s`;
      } else { ttr = "> 1 Hour"; }
    } else if (currentHii >= 0.7) { ttr = "CRITICAL THRESHOLD REACHED"; }

    const confidence = sqi < 50 ? (sqi + Math.floor(Math.random() * 5)) : Math.min(99, sqi - (stdevHR > 10 ? 10 : 0));

    return { score: currentHii * 100, severity, action, trendDirection, sqi, confidence, reasons, contribs: contribs.slice(0, 3), ttr, slopes, stdevHR };
  }

  /* ---------- AI & Trend Engine ---------- */
  function computeTrendPrediction(hii, pId) {
    const hist = patientState[pId].hiiHistory;
    hist.push(hii);
    if (hist.length > 50) hist.shift();

    let trend = "Stable";
    if (hist.length >= 10) {
      if (hist[hist.length - 1] > hist[0] + 0.04) trend = "Increasing (Deterioration)";
      else if (hist[hist.length - 1] < hist[0] - 0.04) trend = "Decreasing (Improvement)";
    }
    return trend;
  }

  function generateAdvancedAI(data, pProfile, pId) {
    const evalAI = evaluatePatientAI(pId);
    if (!evalAI) return { reasons: [], severity: 'stable', summary: 'System collecting baseline data...', confidence: 0 };
    return {
      reasons: evalAI.reasons.map(r => ({ cause: r.label, imp: r.val })),
      severity: evalAI.severity,
      summary: evalAI.action,
      confidence: evalAI.confidence
    };
  }

  /* ---------- update UI from data object ---------- */
  function updateDashboard(data, id) {
    if (id !== currentPatient) return;

    const buf = chartBuffers[id];

    /* push waveform data into chart buffers
       Live mode: ESP32 sends ecg_wave[] and ppg_wave[] arrays (buffered samples)
       Sim mode:  single ecg/ppg values per tick */
    if (Array.isArray(data.ecg_wave) && data.ecg_wave.length > 0) {
      /* batch push — all samples buffered since last poll */
      for (let i = 0; i < data.ecg_wave.length; i++) {
        buf.ecg.push(data.ecg_wave[i]);
        buf.ecg.shift();
      }
      for (let i = 0; i < data.ppg_wave.length; i++) {
        buf.ppg.push(data.ppg_wave[i]);
        buf.ppg.shift();
      }
    } else {
      /* single-point push (simulation mode) */
      buf.ecg.push(data.ecg);
      buf.ecg.shift();
      buf.ppg.push(data.ppg);
      buf.ppg.shift();
    }



    /* update charts */
    ecgChart.data.datasets[0].data = buf.ecg;
    ppgChart.data.datasets[0].data = buf.ppg;
    ecgChart.update('none');
    ppgChart.update('none');

    /* vitals calculated maps */
    let hr, hrv, temp, ptt, bio;

    if (isLiveMode && id === currentPatient) {
      hr = data.hr !== undefined ? data.hr : 0;
      hrv = data.ptt * 0.45;
      temp = data.temp_grad !== undefined ? data.temp_grad : 0.0;
      ptt = data.ptt;
      bio = data.bio;
    } else {
      const state = patientState[id];
      const prog = state.simTick % 3000 / 2000;
      let basePulse = 70;
      if (state.type === 'warning') basePulse = 95;
      if (state.type === 'critical') basePulse = 120;

      hr = basePulse + (prog * 35) + (Math.random() * 4 - 2);
      hrv = 150 - (prog * 45) + (Math.random() * 10 - 5);
      temp = data.temp_grad !== undefined ? data.temp_grad : 0.0;
      ptt = data.ptt;
      bio = data.bio;
    }

    /* update sensor statuses in sidebar based on signal quality */
    const ppgConnected = (data.ppg_amp !== undefined && data.ppg_amp >= 10);
    const ecgConnected = (data.ecg !== undefined && data.ecg > 500 && data.ecg < 4000);
    updateSensorStatus('sens-ecg', ecgConnected);
    updateSensorStatus('sens-ppg', ppgConnected);
    updateSensorStatus('sens-temp', true);
    updateSensorStatus('sens-bio', true);

    /* output vitals */
    dom.hr.textContent = (typeof hr === 'number' && hr > 0) ? hr.toFixed(1) : (hr || '—');
    dom.hrv.textContent = hrv > 0 ? hrv.toFixed(1) : '—';
    dom.temp.textContent = temp.toFixed(1);
    dom.ptt.textContent = ptt.toFixed(1);
    dom.bio.textContent = (bio === 0) ? '—' : bio.toFixed(2);

    prevValues = { ...data };

    /* AI Clinical Engine Update */
    const advAI = generateAdvancedAI(data, patients[id], id);

    dom.aiReasonsList.innerHTML = "";
    advAI.reasons.forEach(r => {
      const div = document.createElement("div");
      div.className = "ai-reason-item " + advAI.severity;
      div.innerHTML = `• ${r.cause} <span>→</span> <strong>${r.imp}</strong>`;
      dom.aiReasonsList.appendChild(div);
    });

    dom.aiConfValue.textContent = advAI.confidence + "%";

    dom.aiSummaryBox.className = "ai-summary-box " + advAI.severity;
    dom.aiSysText.className = "ai-sys-text " + advAI.severity;
    dom.aiSysText.textContent = advAI.summary;

    // Trigger pulse animation
    dom.aiClinicalCard.classList.remove('pulse-anim');
    void dom.aiClinicalCard.offsetWidth; // Reflow
    dom.aiClinicalCard.classList.add('pulse-anim');

    /* Alert Banner Update (Legacy hook) */
    const trendStr = computeTrendPrediction(data.hii, id);
    dom.alertTitle.textContent = advAI.severity.toUpperCase() + (advAI.severity === 'stable' ? '' : ' RISK');
    dom.alertDesc.textContent = advAI.summary + " Trend: " + trendStr + ".";
    dom.alertBanner.className = 'alert-banner ' + advAI.severity;

    // 🔥 ML FEATURES EXTRACTION — only when AI view is active
    if (viewAi && viewAi.style.display !== 'none' && mlChartsInitialized) {
      const slopes = calcBufferSlopes(patientState[id].timeBuf);
      const features = {
        HR: hr,
        HR_slope: slopes.hr,
        PTT: ptt,
        PTT_slope: slopes.ptt,
        PPG: data.ppg || 0,
        PPG_slope: 0,
        Imp: bio,
        Imp_slope: slopes.bio,
        Temp: temp || 36.5,
        Temp_slope: 0
      };

      getAIResult(features).then(result => {
        if (result) {
          updateMLPredictionsUI(result);
        }
      });
    }
  }

  /* ---------- simulation engine ---------- */
  function generateSimData(pId) {
    const state = patientState[pId];
    state.simTick++;
    const p = patients[pId];
    const t = state.simTick * 0.01;
    const phase = state.simTick % 3000;

    let maxProg = 0.1;
    if (state.type === 'warning') maxProg = 0.6;
    if (state.type === 'critical') maxProg = 1.0;

    /* progression factor */
    const progression = Math.min((phase / 2000) * maxProg, maxProg);

    /* ECG */
    const heartCycle = state.simTick % 18;
    let ecg = p.baseECG;
    if (heartCycle === 0) ecg += rand(700, 1000) * rand(0.98, 1.02);
    else if (heartCycle === 1) ecg -= rand(150, 300) * rand(0.98, 1.02);
    else if (heartCycle === 8) ecg += rand(100, 200) * rand(0.98, 1.02);
    else ecg += rand(-40, 40);

    /* PPG */
    const ppgCycle = Math.sin(t * 5.5) * 3000 + Math.sin(t * 11) * 800;
    const ppg = p.basePPG + ppgCycle + rand(-250, 250);

    /* vitals (slight physiological jitter) */
    const ppg_amp = clamp(p.basePPGAmp * (1 - progression * 0.55) + rand(-40, 40), 200, 2000);
    const ptt = clamp(p.basePTT * (1 - progression * 0.35) + rand(-10, 10), 80, 350);
    const temp_grad = clamp(p.baseTempGrad + progression * 3.5 + rand(-0.15, 0.15), 0, 5);
    const bio = clamp(p.baseBio + progression * 80 + rand(-5, 5), 20, 200);

    /* HII */
    const w1 = 0.30, w2 = 0.20, w3 = 0.20, w4 = 0.15, w5 = 0.15;
    const hii = clamp(
      w1 * (1 - ppg_amp / p.basePPGAmp) +
      w2 * (1 - ptt / p.basePTT) +
      w3 * (temp_grad / 5.0) +
      w4 * (bio / 200.0) +
      w5 * progression,
      0, 1
    );

    let status;
    if (hii < 0.3) status = 'STABLE';
    else if (hii < 0.6) status = 'EARLY RISK';
    else status = 'CRITICAL';

    const sensorDrop = progression > 0.85;

    return {
      ecg, ppg, ppg_amp, ptt, temp_grad, bio, hii, status,
      ecg_detected: !sensorDrop || Math.random() > 0.3,
      ppg_detected: !sensorDrop || Math.random() > 0.2,
      temp_detected: !sensorDrop || Math.random() > 0.1,
      bio_detected: !sensorDrop || Math.random() > 0.15,
    };
  }

  /* ---------- live data fetch ---------- */
  let failCount = 0;

  async function fetchLiveData() {
    let ip = dom.ipInput.value.trim();
    // Default to the host IP if empty (perfect for when hosted directly on the ESP32)
    if (!ip) ip = window.location.host; 
    
    try {
      const res = await fetch(`http://${ip}/data`, { signal: AbortSignal.timeout(4000) });
      if (!res.ok) throw new Error(res.status);
      failCount = 0;
      setConnectionStatus('connected');
      return await res.json();
    } catch {
      failCount++;
      /* only show disconnected after 3 consecutive failures
         (ESP32 can be slow when busy with sensors) */
      if (failCount >= 3) setConnectionStatus('disconnected');
      return null;
    }
  }

  function setConnectionStatus(state) {
    // In new UI, we can just optionally update the LIVE indicator or SYSTEM ACTIVE dot
    const isConn = state === 'connected';
    dom.modeLabel.textContent = isConn ? 'SYSTEM CONNECTED' : (state === 'simulating' ? 'SIMULATION ACTIVE' : 'DISCONNECTED');
  }

  function updateSensorStatus(domId, isConnected) {
     const dot = document.querySelector(`#${domId} .dot`);
     if (dot) {
        dot.className = isConnected ? 'dot green-dot' : 'dot red-dot';
     }
  }

  /* ---------- HOSPITAL OVERVIEW LOGIC ---------- */
  const navDash = $('#nav-dashboard');
  const navPatients = $('#nav-patients');
  const navAi = $('#nav-ai');
  const viewDash = $('#view-dashboard');
  const viewPatients = $('#view-patients');
  const viewAi = $('#view-ai');

  let aiChartsInitialized = false;
  const aiCharts = { ecg: null, ppg: null, trend: null };

  /* ---------- ML Model State ---------- */
  let mlChartsInitialized = false;
  const mlCharts = { gauge: null, feature: null, history: null };
  let mlModelInfo = null;
  let mlModelInfoFetched = false;
  const mlPredictionHistory = { confidence: [], classes: [], labels: [] };
  const ML_HISTORY_MAX = 60;

  /* Smoothed ML values to reduce flicker */
  let smoothedMLConf = null;
  let smoothedMLProbs = null;
  const ML_SMOOTH_ALPHA = 0.4;  // higher = more responsive to changes

  function switchView(view) {
    if (viewDash) viewDash.style.display = 'none';
    if (viewPatients) viewPatients.style.display = 'none';
    if (viewAi) viewAi.style.display = 'none';

    if (navDash) navDash.classList.remove('active');
    if (navPatients) navPatients.classList.remove('active');
    if (navAi) navAi.classList.remove('active');

    if (view === 'dashboard') {
      if (viewDash) viewDash.style.display = 'block';
      if (navDash) navDash.classList.add('active');
    } else if (view === 'patients') {
      if (viewPatients) viewPatients.style.display = 'block';
      if (navPatients) navPatients.classList.add('active');
      updatePatientsOverview();
    } else if (view === 'ai') {
      if (viewAi) viewAi.style.display = 'block';
      if (navAi) navAi.classList.add('active');
      initAICharts();
    }
  }

  function initOverviewGrid() {
    const grid = $('#patients-grid');
    if (!grid) return;
    grid.innerHTML = '';

    Object.keys(patients).forEach(pId => {
      const d = document.createElement('div');
      d.id = 'overview-' + pId;
      d.className = 'patient-card';
      const pInfo = patients[pId];
      const condTags = (pInfo.conditions || []).map(c => `<span class="cond-tag">${c}</span>`).join('');
      const medList = (pInfo.medications || []).map(m => `<span class="med-tag">${m}</span>`).join('');
      const allergyList = (pInfo.allergies || []).map(a => `<span class="allergy-tag">${a}</span>`).join('');
      d.innerHTML = `
          <div class="pc-info">
            <div class="pc-name">${pInfo.name}</div>
            <div class="pc-details" style="font-size:0.75rem; color:var(--text-muted); margin:4px 0 6px;">
              ${pInfo.age || ''}${pInfo.sex ? ' / ' + pInfo.sex : ''}
              ${pInfo.bloodType ? ' · <strong>' + pInfo.bloodType + '</strong>' : ''}
              ${pInfo.room ? ' · Room: ' + pInfo.room : ''}
            </div>
            <div class="pc-conditions" style="display:flex; flex-wrap:wrap; gap:4px; margin-bottom:6px;">
              ${condTags}
            </div>
            <div class="pc-conditions" style="display:flex; flex-wrap:wrap; gap:4px; margin-bottom:6px;">
              ${medList}
            </div>
            ${allergyList ? '<div class="pc-conditions" style="display:flex; flex-wrap:wrap; gap:4px; margin-bottom:6px;">' + allergyList + '</div>' : ''}
            <div class="pc-status" id="pc-status-${pId}">--</div>
          </div>
          <div class="pc-vitals">
            <div class="pc-vital">
               <div class="pc-vital-val" id="pc-hr-${pId}">--</div>
               <div class="pc-vital-lbl">HEART RATE</div>
            </div>
            <div class="pc-vital">
               <div class="pc-vital-val" id="pc-hii-${pId}">--</div>
               <div class="pc-vital-lbl">HII SCORE</div>
            </div>
          </div>
          <div class="pc-chart">
             <canvas id="pc-canvas-${pId}"></canvas>
          </div>
       `;
      d.addEventListener('click', () => {
        switchPatient(pId);
        switchView('dashboard');
        if (dom.patientSelect) dom.patientSelect.value = pId;
      });
      grid.appendChild(d);

      chartBuffers[pId].miniChart = new Chart($(`#pc-canvas-${pId}`), {
        type: 'line',
        data: {
          labels: new Array(100).fill(0),
          datasets: [{ data: chartBuffers[pId].miniEcg, borderColor: '#38bdf8', backgroundColor: 'rgba(56,189,248,.1)', fill: true }]
        },
        options: { ...chartDefaults, animation: { duration: 0 }, elements: { point: { radius: 0 }, line: { borderWidth: 1.5, tension: 0.3 } } }
      });
    });
  }

  function updatePatientsOverview() {
    if (!viewPatients || viewPatients.style.display === 'none') return;

    Object.keys(patients).forEach(pId => {
      const data = patientState[pId].lastGen;
      if (!data) return;

      const advAI = generateAdvancedAI(data, patients[pId], pId);
      const stateType = patientState[pId].type;

      let hrVal = data.hr !== undefined ? data.hr : 75;
      if (isLiveMode && pId === currentPatient && typeof hrVal === 'number') {
         hrVal = Math.round(hrVal);
      } else if (!isLiveMode || pId !== currentPatient) {
        // Use hardcoded simulation bounds to guarantee visual distinction
        hrVal = Math.round(stateType === 'stable' ? (70 + Math.random() * 5) : (stateType === 'warning' ? (95 + Math.random() * 5) : (120 + Math.random() * 5)));
      }

      const elStatus = $(`#pc-status-${pId}`);
      if (elStatus) {
        elStatus.textContent = advAI.severity.toUpperCase() + (advAI.severity === 'stable' ? '' : ' RISK');
        elStatus.className = 'pc-status ' + advAI.severity;
      }

      const elHr = $(`#pc-hr-${pId}`);
      if (elHr) {
        elHr.textContent = hrVal;
        elHr.className = 'pc-vital-val ' + advAI.severity;
      }

      const elHii = $(`#pc-hii-${pId}`);
      if (elHii) {
        elHii.textContent = data.hii.toFixed(2);
        elHii.className = 'pc-vital-val ' + advAI.severity;
      }

      const card = $(`#overview-${pId}`);
      if (card) {
        if (advAI.severity === 'critical') {
          card.style.order = -1;
          card.classList.add('critical');
        } else if (advAI.severity === 'warning') {
          card.style.order = 0;
          card.classList.remove('critical');
        } else {
          card.style.order = 1;
          card.classList.remove('critical');
        }
      }

      if (chartBuffers[pId].miniChart) {
        chartBuffers[pId].miniChart.update('none');
      }
    });
  }

  /* ---------- AI ANALYSIS RENDERER ---------- */
  function initAICharts() {
    if (aiChartsInitialized) return;
    aiChartsInitialized = true;

    const labels = Array.from({ length: CHART_POINTS }, (_, i) => i);

    aiCharts.ecg = new Chart($('#ai-ecg-chart'), {
      type: 'line', data: { labels, datasets: [{ data: [], borderColor: '#38bdf8', backgroundColor: 'rgba(56,189,248,.08)', fill: true, pointRadius: 0, borderWidth: 1.8, tension: 0.3 }] },
      options: { ...chartDefaults }
    });

    aiCharts.ppg = new Chart($('#ai-ppg-chart'), {
      type: 'line', data: { labels, datasets: [{ data: [], borderColor: '#a78bfa', backgroundColor: 'rgba(167,139,250,.08)', fill: true, pointRadius: 0, borderWidth: 1.8, tension: 0.3 }] },
      options: { ...chartDefaults }
    });

    // Trend chart (rolling time buffer)
    aiCharts.trend = new Chart($('#ai-trend-chart'), {
      type: 'line',
      data: {
        labels: new Array(70).fill(''),
        datasets: [
          { label: 'Smoothed HIS', data: [], borderColor: '#f43f5e', backgroundColor: 'rgba(244,63,94,0.1)', fill: true, tension: 0.4, borderWidth: 2, pointRadius: 0 },
          { label: 'Raw HIS points', data: [], borderColor: 'rgba(255,255,255,0.15)', borderWidth: 1, borderDash: [5, 5], pointRadius: 0, tension: 0.1 }
        ]
      },
      options: {
        ...chartDefaults,
        scales: { x: { display: false }, y: { display: true, min: 0, max: 1, grid: { color: 'rgba(255,255,255,0.05)' }, border: { display: false }, ticks: { display: false } } }
      }
    });

    const advToggle = $('#ai-adv-toggle');
    if (advToggle) {
      advToggle.addEventListener('change', e => {
        $('#ai-trend-container').style.display = e.target.checked ? 'none' : 'block';
        $('#ai-advanced-panel').style.display = e.target.checked ? 'grid' : 'none';
      });
    }

    // --- ML Charts Initialization ---
    initMLCharts();
  }

  /* ---------- ML CHARTS INITIALIZATION ---------- */
  function initMLCharts() {
    if (mlChartsInitialized) return;
    mlChartsInitialized = true;

    // Confidence Gauge (Doughnut)
    mlCharts.gauge = new Chart($('#ml-gauge-chart'), {
      type: 'doughnut',
      data: {
        labels: ['Confidence', 'Remaining'],
        datasets: [{
          data: [0, 100],
          backgroundColor: ['#38bdf8', 'rgba(255,255,255,0.04)'],
          borderWidth: 0,
          cutout: '78%'
        }]
      },
      options: {
        responsive: true,
        maintainAspectRatio: true,
        plugins: { legend: { display: false }, tooltip: { enabled: false } },
        animation: { animateRotate: true, duration: 600 }
      }
    });

    // Feature Importance (Horizontal Bar) — populated after /model-info loads
    mlCharts.feature = new Chart($('#ml-feature-chart'), {
      type: 'bar',
      data: {
        labels: [],
        datasets: [{
          label: 'Importance',
          data: [],
          backgroundColor: [
            'rgba(16,185,129,0.7)', 'rgba(56,189,248,0.7)', 'rgba(251,191,36,0.7)',
            'rgba(167,139,250,0.7)', 'rgba(244,63,94,0.7)', 'rgba(96,165,250,0.7)',
            'rgba(180,142,173,0.7)', 'rgba(16,185,129,0.5)', 'rgba(56,189,248,0.5)',
            'rgba(251,191,36,0.5)'
          ],
          borderColor: [
            '#10b981', '#38bdf8', '#fbbf24', '#a78bfa', '#f43f5e',
            '#60a5fa', '#b48ead', '#10b981', '#38bdf8', '#fbbf24'
          ],
          borderWidth: 1,
          borderRadius: 6,
          barPercentage: 0.7
        }]
      },
      options: {
        indexAxis: 'y',
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
          legend: { display: false },
          tooltip: {
            callbacks: {
              label: ctx => `Importance: ${(ctx.raw * 100).toFixed(1)}%`
            }
          }
        },
        scales: {
          x: {
            display: true,
            grid: { color: 'rgba(255,255,255,0.04)' },
            ticks: { color: '#94a3b8', font: { size: 11, family: 'JetBrains Mono' }, callback: v => (v * 100).toFixed(0) + '%' },
            border: { display: false }
          },
          y: {
            display: true,
            grid: { display: false },
            ticks: { color: '#e2e8f0', font: { size: 11, family: 'Inter', weight: 600 } },
            border: { display: false }
          }
        },
        animation: { duration: 800 }
      }
    });

    // Prediction History (Line chart)
    mlCharts.history = new Chart($('#ml-history-chart'), {
      type: 'line',
      data: {
        labels: new Array(ML_HISTORY_MAX).fill(''),
        datasets: [
          {
            label: 'Confidence',
            data: [],
            borderColor: '#38bdf8',
            backgroundColor: 'rgba(56,189,248,0.1)',
            fill: true,
            tension: 0.4,
            borderWidth: 2,
            pointRadius: 0
          },
          {
            label: 'Predicted Class',
            data: [],
            borderColor: 'rgba(167,139,250,0.6)',
            borderWidth: 1.5,
            borderDash: [4, 4],
            pointRadius: 0,
            tension: 0.3,
            yAxisID: 'y1'
          }
        ]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
          legend: {
            display: true,
            position: 'top',
            labels: { color: '#94a3b8', font: { size: 10 }, boxWidth: 12, padding: 12 }
          }
        },
        scales: {
          x: { display: false },
          y: {
            display: true,
            min: 0, max: 1,
            grid: { color: 'rgba(255,255,255,0.04)' },
            ticks: { color: '#94a3b8', font: { size: 10, family: 'JetBrains Mono' }, callback: v => (v * 100) + '%' },
            border: { display: false }
          },
          y1: {
            display: true,
            position: 'right',
            min: 0, max: 2,
            grid: { display: false },
            ticks: {
              color: '#94a3b8',
              font: { size: 10 },
              stepSize: 1,
              callback: v => ['STABLE', 'WARN', 'CRIT'][v] || ''
            },
            border: { display: false }
          }
        },
        animation: { duration: 0 }
      }
    });

    // Fetch model info once
    fetchModelInfo();
  }

  /* ---------- FETCH MODEL INFO ---------- */
  async function fetchModelInfo() {
    if (mlModelInfoFetched) return;
    try {
      const res = await fetch('http://127.0.0.1:8000/model-info');
      const info = await res.json();
      if (info.error) { console.warn('Model info error:', info.error); return; }
      mlModelInfo = info;
      mlModelInfoFetched = true;

      // Populate feature importance chart
      if (mlCharts.feature && info.feature_names && info.feature_importances) {
        // Sort by importance descending
        const paired = info.feature_names.map((n, i) => ({ name: n, imp: info.feature_importances[i] }));
        paired.sort((a, b) => b.imp - a.imp);
        mlCharts.feature.data.labels = paired.map(p => p.name);
        mlCharts.feature.data.datasets[0].data = paired.map(p => p.imp);
        mlCharts.feature.update();
      }

      // Populate model metadata
      const metaType = $('#ml-meta-type');
      if (metaType) metaType.textContent = info.model_type || '—';

      const metaLstmArch = $('#ml-meta-lstm');
      const rowLstm = $('#meta-row-lstm');
      if (metaLstmArch && rowLstm && info.hybrid_mode && info.lstm_architecture) {
        metaLstmArch.textContent = info.lstm_architecture;
        rowLstm.style.display = 'block';
      }

      const metaSeq = $('#ml-meta-seq');
      const rowSeq = $('#meta-row-seq');
      if (metaSeq && rowSeq && info.hybrid_mode && info.seq_len) {
        metaSeq.textContent = info.seq_len;
        rowSeq.style.display = 'block';
      }

      const metaEst = $('#ml-meta-estimators');
      if (metaEst) metaEst.textContent = info.n_estimators || '—';
      const metaFeat = $('#ml-meta-features');
      if (metaFeat) metaFeat.textContent = info.n_features || '—';
      const metaDepth = $('#ml-meta-depth');
      if (metaDepth) metaDepth.textContent = info.max_depth || '—';
      const metaClasses = $('#ml-meta-classes');
      if (metaClasses) metaClasses.textContent = (info.class_labels || []).join(', ');
      const metaStatus = $('#ml-meta-status');
      if (metaStatus) metaStatus.textContent = '✅ Loaded';

    } catch (err) {
      console.warn('Could not fetch model-info:', err);
      const metaStatus = $('#ml-meta-status');
      if (metaStatus) { metaStatus.textContent = '❌ Offline'; metaStatus.style.color = '#f43f5e'; }
      // Retry after 3 seconds if not yet fetched
      setTimeout(() => fetchModelInfo(), 3000);
    }
  }

  /* ---------- UPDATE ML PREDICTIONS UI ---------- */
  function updateMLPredictionsUI(result) {
    if (!result || result.error) return;

    /* --- Dynamic Feature Importance Update (jitter to show activity) --- */
    if (mlCharts.feature && mlModelInfo && mlModelInfo.feature_importances) {
      const baseImps = mlModelInfo.feature_importances;
      const jittered = baseImps.map(v => v * (1 + (Math.random() - 0.5) * 0.06));
      // re-sort paired
      const paired = mlModelInfo.feature_names.map((n, i) => ({ name: n, imp: jittered[i] }));
      paired.sort((a, b) => b.imp - a.imp);
      mlCharts.feature.data.labels = paired.map(p => p.name);
      mlCharts.feature.data.datasets[0].data = paired.map(p => p.imp);
      mlCharts.feature.update();
    }

    const pred = result.prediction;   // 0, 1, 2
    const label = result.label || ['STABLE', 'WARNING', 'CRITICAL'][pred];
    const rawProbs = result.probability;  // [stable%, warn%, crit%]
    const rawConf = result.confidence;    // 0..1

    // Apply EMA smoothing to reduce flicker
    if (smoothedMLConf === null) {
      smoothedMLConf = rawConf;
      smoothedMLProbs = [...rawProbs];
    } else {
      smoothedMLConf = ML_SMOOTH_ALPHA * rawConf + (1 - ML_SMOOTH_ALPHA) * smoothedMLConf;
      smoothedMLProbs = rawProbs.map((p, i) => ML_SMOOTH_ALPHA * p + (1 - ML_SMOOTH_ALPHA) * smoothedMLProbs[i]);
    }
    const conf = smoothedMLConf;
    const probs = smoothedMLProbs;

    const severity = pred === 0 ? 'stable' : (pred === 1 ? 'warning' : 'critical');

    // 1. Prediction Card
    const predCard = $('#ml-prediction-card');
    if (predCard) {
      predCard.className = 'ml-prediction-card ' + severity;
    }
    const predClass = $('#ml-pred-class');
    if (predClass) predClass.textContent = label;
    const predBadge = $('#ml-pred-badge');
    if (predBadge) {
      predBadge.textContent = severity === 'stable' ? 'NORMAL RANGE' : (severity === 'warning' ? 'ELEVATED RISK' : 'HIGH RISK');
      predBadge.className = 'ml-pred-badge ' + severity;
    }
    const predConf = $('#ml-pred-conf');
    if (predConf) predConf.textContent = (conf * 100).toFixed(1) + '%';

    // 2. Gauge Chart
    if (mlCharts.gauge) {
      const confPct = conf * 100;
      mlCharts.gauge.data.datasets[0].data = [confPct, 100 - confPct];
      const gaugeColor = severity === 'stable' ? '#10b981' : (severity === 'warning' ? '#fbbf24' : '#f43f5e');
      mlCharts.gauge.data.datasets[0].backgroundColor = [gaugeColor, 'rgba(255,255,255,0.04)'];
      mlCharts.gauge.update();
    }
    const gaugeLabel = $('#ml-gauge-label');
    if (gaugeLabel) gaugeLabel.textContent = (conf * 100).toFixed(0) + '%';

    // 3. Probability Bars
    if (probs && probs.length >= 3) {
      const stableBar = $('#ml-prob-stable');
      const warnBar = $('#ml-prob-warning');
      const critBar = $('#ml-prob-critical');
      if (stableBar) stableBar.style.width = (probs[0] * 100).toFixed(1) + '%';
      if (warnBar) warnBar.style.width = (probs[1] * 100).toFixed(1) + '%';
      if (critBar) critBar.style.width = (probs[2] * 100).toFixed(1) + '%';

      const stablePct = $('#ml-prob-stable-pct');
      const warnPct = $('#ml-prob-warning-pct');
      const critPct = $('#ml-prob-critical-pct');
      if (stablePct) stablePct.textContent = (probs[0] * 100).toFixed(1) + '%';
      if (warnPct) warnPct.textContent = (probs[1] * 100).toFixed(1) + '%';
      if (critPct) critPct.textContent = (probs[2] * 100).toFixed(1) + '%';
    }

    // 4. Prediction History
    mlPredictionHistory.confidence.push(conf);
    mlPredictionHistory.classes.push(pred);
    const now = new Date();
    mlPredictionHistory.labels.push(now.toLocaleTimeString('en-US', { hour12: false }));
    while (mlPredictionHistory.confidence.length > ML_HISTORY_MAX) {
      mlPredictionHistory.confidence.shift();
      mlPredictionHistory.classes.shift();
      mlPredictionHistory.labels.shift();
    }

    if (mlCharts.history) {
      mlCharts.history.data.labels = mlPredictionHistory.labels;
      mlCharts.history.data.datasets[0].data = mlPredictionHistory.confidence;
      mlCharts.history.data.datasets[1].data = mlPredictionHistory.classes;
      mlCharts.history.update('none');
    }
  }

  function updateAIDashboard() {
    if (!viewAi || viewAi.style.display === 'none' || !aiChartsInitialized) return;

    const pId = currentPatient;
    const data = patientState[pId].lastGen;
    const buf = patientState[pId].timeBuf;
    if (!data || buf.length === 0) return;

    // 1. Top Vitals Row
    const hrVal = data.hr !== undefined ? data.hr : 75;
    $('#ai-v-hr').textContent = (typeof hrVal === 'number') ? Math.round(hrVal) : hrVal;
    $('#ai-v-hrv').textContent = (data.ptt * 0.45).toFixed(1);
    $('#ai-v-temp').textContent = (data.temp1 || 36.8).toFixed(1);
    $('#ai-v-ptt').textContent = data.ptt.toFixed(1);
    $('#ai-v-bio').textContent = (data.bio === 0) ? '—' : data.bio.toFixed(2);

    // 2. Charts
    const cBuf = chartBuffers[pId];
    aiCharts.ecg.data.datasets[0].data = cBuf.ecg;
    aiCharts.ppg.data.datasets[0].data = cBuf.ppg;
    aiCharts.ecg.update('none');
    aiCharts.ppg.update('none');

    // 3. AI Evaluations
    const evalAI = evaluatePatientAI(pId);
    if (!evalAI) return;

    // SQI
    $('#ai-ecg-sqi').textContent = `SQI: ${evalAI.sqi}%`;
    $('#ai-ppg-sqi').textContent = `SQI: ${evalAI.sqi}%`;

    // Contributions
    const contribList = $('#ai-contrib-list');
    if (contribList) {
      contribList.innerHTML = '';
      evalAI.contribs.forEach(c => {
        contribList.innerHTML += `<div class="contrib-item"><div>${c.name}</div><div>${Math.round(c.weight)}%</div></div>`;
      });
    }

    // Score Panel
    $('#ai-his-val').textContent = Math.round(evalAI.score);
    $('#ai-his-val').className = 'ai-score-val ' + evalAI.severity;
    $('#ai-his-trend').textContent = evalAI.trendDirection;
    $('#ai-his-conf').textContent = `CONF: ${Math.round(evalAI.confidence)}%`;

    // Decision Panel
    const decBox = $('#ai-decision-box');
    decBox.className = 'ai-decision-panel ' + evalAI.severity;
    $('#ai-decision-status').textContent = evalAI.severity.toUpperCase();
    $('#ai-decision-status').className = 'ai-decision-status ' + evalAI.severity;
    $('#ai-decision-action').textContent = evalAI.action;
    $('#ai-decision-time').textContent = "Estimated progression to critical state: " + evalAI.ttr;

    // Interpretations
    const interpList = $('#ai-interp-list');
    if (interpList) {
      interpList.innerHTML = '';
      evalAI.reasons.forEach(r => {
        interpList.innerHTML += `<div class="ai-reason-item ${evalAI.severity}">• ${r.label} <span>→</span> <strong>${r.val}</strong></div>`;
      });
    }

    // Trend Graph
    aiCharts.trend.data.datasets[0].data = buf.map(b => b.emaHii || b.hii);
    aiCharts.trend.data.datasets[1].data = buf.map(b => b.hii);
    aiCharts.trend.update('none');

    // Advanced Panel
    $('#adv-hr-sl').textContent = evalAI.slopes.hr.toFixed(2);
    $('#adv-ptt-sl').textContent = evalAI.slopes.ptt.toFixed(2);
    $('#adv-bio-sl').textContent = evalAI.slopes.bio.toFixed(2);
    $('#adv-hr-var').textContent = evalAI.stdevHR.toFixed(2);
    $('#adv-base-lock').textContent = patientState[pId].base ? "Yes" : "Calibrating";
  }

  /* ---------- main loop ---------- */
  function startLoop() {
    stopLoop();
    const interval = isLiveMode ? POLL_INTERVAL : SIM_INTERVAL;

    intervalId = setInterval(async () => {
      let liveData = null;
      if (isLiveMode) {
        liveData = await fetchLiveData();
      } else {
        setConnectionStatus('simulating');
      }

      const now = Date.now();

      Object.keys(patients).forEach(pId => {
        let data;
        if (isLiveMode && pId === currentPatient) {
          data = liveData;
        } else {
          data = generateSimData(pId);
        }

        if (data) {
          patientState[pId].lastGen = data;

          // Maintain 10-second Rolling Time Buffer
          const stType = patientState[pId].type;
          let hrVal = Math.round(data.rr_interval ? (60000 / data.rr_interval) : (data.ptt > 0 ? (60000 / data.ptt) : 75));
          if (!data.rr_interval && (!isLiveMode || pId !== currentPatient)) hrVal = Math.round(stType === 'stable' ? (70 + Math.random() * 5) : (stType === 'warning' ? (95 + Math.random() * 5) : (120 + Math.random() * 5)));

          patientState[pId].timeBuf.push({ t: now, hr: hrVal, ptt: data.ptt, bio: data.bio, hii: data.hii });
          while (patientState[pId].timeBuf.length > 0 && now - patientState[pId].timeBuf[0].t > 10500) {
            patientState[pId].timeBuf.shift();
          }

          // Automatic Baseline Lock Mechanism
          if (!patientState[pId].base && patientState[pId].timeBuf.length > 40) {
            const b = patientState[pId].timeBuf;
            patientState[pId].base = {
              hr: b.reduce((s, x) => s + x.hr, 0) / b.length,
              ptt: b.reduce((s, x) => s + x.ptt, 0) / b.length,
              bio: b.reduce((s, x) => s + x.bio, 0) / b.length
            };
          }

          // Push scalar ECG to mini buffer
          const ecgVal = Array.isArray(data.ecg_wave) && data.ecg_wave.length > 0 ? data.ecg_wave[data.ecg_wave.length - 1] : data.ecg;
          chartBuffers[pId].miniEcg.push(ecgVal);
          chartBuffers[pId].miniEcg.shift();

          if (pId === currentPatient) {
            updateDashboard(data, pId);
          }
        }
      });

      updatePatientsOverview();
      updateAIDashboard();

      const ts = $('#ai-timestamp');
      if (ts) ts.textContent = new Date().toLocaleTimeString('en-US', { hour12: false }) + '.' + String(new Date().getMilliseconds()).padStart(3, '0');

    }, interval);
  }

  function stopLoop() {
    if (intervalId) { clearInterval(intervalId); intervalId = null; }
  }

  /* ---------- chart buffer swap on patient change ---------- */
  function switchPatient(id) {
    if (id === currentPatient) return;
    currentPatient = id;

    ecgChart.data.datasets[0].data = chartBuffers[id].ecg;
    ppgChart.data.datasets[0].data = chartBuffers[id].ppg;
    ecgChart.update('none');
    ppgChart.update('none');
  }

  /* ---------- event listeners ---------- */
  dom.patientSelect.addEventListener('change', e => {
    switchPatient(e.target.value);
  });

  dom.modeToggle.addEventListener('click', () => {
    isLiveMode = !isLiveMode;
    dom.modeToggle.className = 'mode-toggle ' + (isLiveMode ? 'live' : 'sim');
    dom.modeLabel.textContent = isLiveMode ? 'LIVE MODE' : 'SIMULATION';
    startLoop();
  });

  /* ---------- Sensor Status Dot Helper ---------- */
  function updateSensorStatus(elemId, isConnected) {
    const el = document.getElementById(elemId);
    if (!el) return;
    const dot = el.querySelector('.dot');
    if (!dot) return;
    if (isConnected) {
      dot.classList.add('green-dot');
      dot.classList.remove('red-dot');
    } else {
      dot.classList.add('red-dot');
      dot.classList.remove('green-dot');
    }
  }

  /* ---------- boot ---------- */
  function init() {
    if (navDash) navDash.addEventListener('click', (e) => { e.preventDefault(); switchView('dashboard'); });
    if (navPatients) navPatients.addEventListener('click', (e) => { e.preventDefault(); switchView('patients'); });
    if (navAi) navAi.addEventListener('click', (e) => { e.preventDefault(); switchView('ai'); });

    initOverviewGrid();

    dom.modeToggle.className = 'mode-toggle sim';
    dom.modeLabel.textContent = 'SIMULATION';
    setConnectionStatus('simulating');
    startLoop();
  }

  init();
})();
