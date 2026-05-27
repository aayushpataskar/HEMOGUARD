/* ============================================================
   HEMOGUARD — Dashboard Logic
   Live ESP32 data  ·  Simulation engine  ·  Chart.js waveforms
   ============================================================ */

(() => {
  'use strict';

  /* ---------- constants ---------- */
  const CHART_POINTS   = 200;
  const POLL_INTERVAL  = 150;   // ms — live mode
  const SIM_INTERVAL   = 100;   // ms — simulation tick

  /* ---------- patient profiles ---------- */
  const patients = {
    'patient-1': {
      name: 'Patient 1 — John D.',
      baseECG: 2100, basePPG: 51000, basePPGAmp: 1300,
      basePTT: 230, baseTempGrad: 0.8, baseBio: 45,
    },
    'patient-2': {
      name: 'Patient 2 — Sarah M.',
      baseECG: 1950, basePPG: 48000, basePPGAmp: 1100,
      basePTT: 210, baseTempGrad: 1.0, baseBio: 50,
    },
    'patient-3': {
      name: 'Patient 3 — Raj K.',
      baseECG: 2250, basePPG: 53000, basePPGAmp: 1500,
      basePTT: 250, baseTempGrad: 0.5, baseBio: 38,
    },
  };

  /* ---------- state ---------- */
  let currentPatient = 'patient-1';
  let isLiveMode     = false;
  let intervalId     = null;
  let simTick        = 0;
  let prevValues     = {};

  /* per-patient chart data buffers */
  const chartBuffers = {};
  Object.keys(patients).forEach(id => {
    chartBuffers[id] = {
      ecg: new Array(CHART_POINTS).fill(0),
      ppg: new Array(CHART_POINTS).fill(0),
    };
  });

  /* ---------- DOM refs ---------- */
  const $  = s => document.querySelector(s);
  const $$ = s => document.querySelectorAll(s);

  const dom = {
    patientSelect : $('#patient-select'),
    modeToggle    : $('#mode-toggle'),
    modeLabel     : $('#mode-label'),
    ipInput       : $('#esp-ip'),
    connBadge     : $('#conn-badge'),
    connText      : $('#conn-text'),
    timestamp     : $('#timestamp'),

    ecgCanvas     : $('#ecg-chart'),
    ppgCanvas     : $('#ppg-chart'),

    statusBox     : $('#status-box'),
    hiiValue      : $('#hii-value'),

    ppgAmp        : $('#v-ppgamp'),
    ptt           : $('#v-ptt'),
    tempGrad      : $('#v-tempgrad'),
    bio           : $('#v-bio'),
    hii           : $('#v-hii'),

    trendPPGAmp   : $('#t-ppgamp'),
    trendPTT      : $('#t-ptt'),
    trendTempGrad : $('#t-tempgrad'),
    trendBio      : $('#t-bio'),
    trendHII      : $('#t-hii'),

    sensorECG     : $('#sensor-ecg'),
    sensorPPG     : $('#sensor-ppg'),
    sensorTemp    : $('#sensor-temp'),
    sensorBio     : $('#sensor-bio'),
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
      line:  { borderWidth: 1.8, tension: 0.3 },
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

  /* ---------- update UI from data object ---------- */
  function updateDashboard(data) {
    const buf = chartBuffers[currentPatient];

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

    /* vitals */
    dom.ppgAmp.textContent   = data.ppg_amp.toFixed(0);
    dom.ptt.textContent      = data.ptt.toFixed(0);
    dom.tempGrad.textContent = data.temp_grad.toFixed(2);
    dom.bio.textContent      = data.bio.toFixed(0);
    dom.hii.textContent      = data.hii.toFixed(3);

    /* trends */
    setTrend(dom.trendPPGAmp,   data.ppg_amp,   prevValues.ppg_amp);
    setTrend(dom.trendPTT,      data.ptt,       prevValues.ptt);
    setTrend(dom.trendTempGrad, data.temp_grad,  prevValues.temp_grad);
    setTrend(dom.trendBio,      data.bio,        prevValues.bio);
    setTrend(dom.trendHII,      data.hii,        prevValues.hii);
    prevValues = { ...data };

    /* status */
    const status = data.status.toUpperCase();
    dom.statusBox.textContent = status;
    dom.hiiValue.textContent  = data.hii.toFixed(3);

    dom.statusBox.className = 'status-box';
    if (status.includes('STABLE'))      dom.statusBox.classList.add('stable');
    else if (status.includes('EARLY'))  dom.statusBox.classList.add('early');
    else if (status.includes('CRITICAL')) dom.statusBox.classList.add('critical');
    else if (status.includes('INSUFFICIENT')) dom.statusBox.classList.add('early');
    else                                dom.statusBox.classList.add('critical');

    /* sensor status */
    updateSensor(dom.sensorECG,  data.ecg_detected);
    updateSensor(dom.sensorPPG,  data.ppg_detected);
    updateSensor(dom.sensorTemp, data.temp_detected);
    updateSensor(dom.sensorBio,  data.bio_detected);

    /* timestamp */
    dom.timestamp.textContent = formatTime();
  }

  function updateSensor(row, detected) {
    const dot  = row.querySelector('.sensor-dot');
    const text = row.querySelector('.sensor-status-text');
    if (detected) {
      dot.className  = 'sensor-dot active';
      text.className = 'sensor-status-text active';
      text.textContent = 'Detected';
    } else {
      dot.className  = 'sensor-dot inactive';
      text.className = 'sensor-status-text inactive';
      text.textContent = 'No Signal';
    }
  }

  /* ---------- simulation engine ---------- */
  function generateSimData() {
    simTick++;
    const p = patients[currentPatient];
    const t = simTick * 0.01;        // normalized time
    const phase = simTick % 3000;    // cycle every ~5 min

    /* progression factor — ramps up then resets, simulating deterioration */
    const progression = Math.min(phase / 2000, 1.0);

    /* ECG: realistic R-peak rhythm */
    const heartCycle = simTick % 18;
    let ecg = p.baseECG;
    if (heartCycle === 0)           ecg += rand(700, 1000);   // R peak
    else if (heartCycle === 1)      ecg -= rand(150, 300);    // S wave
    else if (heartCycle === 8)      ecg += rand(100, 200);    // T wave
    else                            ecg += rand(-40, 40);     // baseline

    /* PPG: pulsatile wave */
    const ppgCycle = Math.sin(t * 5.5) * 3000 + Math.sin(t * 11) * 800;
    const ppg = p.basePPG + ppgCycle + rand(-200, 200);

    /* deteriorating vitals */
    const ppg_amp   = clamp(p.basePPGAmp   * (1 - progression * 0.55) + rand(-30, 30), 200, 2000);
    const ptt       = clamp(p.basePTT      * (1 - progression * 0.35) + rand(-5, 5), 80, 350);
    const temp_grad = clamp(p.baseTempGrad + progression * 3.5 + rand(-0.1, 0.1), 0, 5);
    const bio       = clamp(p.baseBio      + progression * 80 + rand(-3, 3), 20, 200);

    /* HII — weighted composite */
    const w1 = 0.30, w2 = 0.20, w3 = 0.20, w4 = 0.15, w5 = 0.15;
    const hii = clamp(
      w1 * (1 - ppg_amp  / p.basePPGAmp) +
      w2 * (1 - ptt      / p.basePTT) +
      w3 * (temp_grad    / 5.0) +
      w4 * (bio          / 200.0) +
      w5 * progression,
      0, 1
    );

    let status;
    if (hii < 0.3)      status = 'STABLE';
    else if (hii < 0.6) status = 'EARLY RISK';
    else                status = 'CRITICAL';

    /* sensors randomly drop */
    const sensorDrop = progression > 0.85;

    return {
      ecg, ppg, ppg_amp, ptt, temp_grad, bio, hii, status,
      ecg_detected:  !sensorDrop || Math.random() > 0.3,
      ppg_detected:  !sensorDrop || Math.random() > 0.2,
      temp_detected: !sensorDrop || Math.random() > 0.1,
      bio_detected:  !sensorDrop || Math.random() > 0.15,
    };
  }

  /* ---------- live data fetch ---------- */
  let failCount = 0;

  async function fetchLiveData() {
    const ip = dom.ipInput.value.trim();
    if (!ip) return null;
    try {
      const res = await fetch(`http://${ip}/data`, { signal: AbortSignal.timeout(3000) });
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
    dom.connBadge.className = 'conn-badge ' + state;
    const labels = { connected: 'Connected', disconnected: 'Disconnected', simulating: 'Simulating' };
    dom.connText.textContent = labels[state] || state;
  }

  /* ---------- main loop ---------- */
  function startLoop() {
    stopLoop();
    const interval = isLiveMode ? POLL_INTERVAL : SIM_INTERVAL;

    intervalId = setInterval(async () => {
      let data;
      if (isLiveMode) {
        data = await fetchLiveData();
        if (!data) return;
      } else {
        data = generateSimData();
        setConnectionStatus('simulating');
      }
      updateDashboard(data);
    }, interval);
  }

  function stopLoop() {
    if (intervalId) { clearInterval(intervalId); intervalId = null; }
  }

  /* ---------- chart buffer swap on patient change ---------- */
  function switchPatient(id) {
    currentPatient = id;
    simTick = 0;
    prevValues = {};
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

  /* ---------- boot ---------- */
  function init() {
    dom.modeToggle.className = 'mode-toggle sim';
    dom.modeLabel.textContent = 'SIMULATION';
    setConnectionStatus('simulating');
    startLoop();
  }

  init();
})();
