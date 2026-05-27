#ifndef PAGE_HTML_H
#define PAGE_HTML_H

const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>HemoGuard AI — Hemorrhage Detection</title>
  <meta name="description" content="Real-time wireless biomedical monitoring dashboard." />
  <link rel="stylesheet" href="style2.css" />
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.7/dist/chart.umd.min.js"></script>
</head>
<body>

  <div class="dashboard-wrapper">
    
    <!-- ═══════ SIDEBAR ═══════ -->
    <aside class="sidebar">
      <div class="sidebar-brand">
        <div class="brand-title">HemoGuard AI</div>
        <div class="brand-subtitle">HEMORRHAGE DETECTION</div>
      </div>
      
      <div class="nav-section">
        <div class="nav-header">NAVIGATION</div>
        <a href="#" class="nav-item"><span class="nav-icon">⊞</span> Dashboard</a>
      </div>

      <div class="nav-section">
        <div class="nav-header">MONITORING</div>
        <a href="#" class="nav-item active" id="nav-dashboard"><span class="nav-icon">~</span> Live Monitoring</a>
        <a href="#" class="nav-item" id="nav-patients"><span class="nav-icon">👥</span> Patients</a>
        <a href="#" class="nav-item"><span class="nav-icon">🕒</span> History</a>
        <a href="#" class="nav-item"><span class="nav-icon">🔔</span> Alerts</a>
        <a href="#" class="nav-item" id="nav-ai"><span class="nav-icon">🧠</span> AI Analysis</a>
      </div>

      <div class="sidebar-footer">
        <div class="nav-header">SYSTEM STATUS</div>
        <div class="sys-status online"><span class="dot green-dot"></span> Online</div>
      </div>
    </aside>

    <!-- ═══════ MAIN CONTENT ═══════ -->
    <main class="main-content">
      
      <!-- ═══════ DASHBOARD VIEW ═══════ -->
      <div id="view-dashboard" class="app-view active">
        <!-- TOP BAR -->
      <header class="top-bar">
        <div class="top-title">
          <span class="icon-shield">⛨</span> Patient Monitoring
        </div>

        <div class="top-controls">
          <span class="control-label">DATA SOURCE:</span>
          <select id="patient-select" class="dropdown light">
            <option value="patient-1">Pt 1</option>
            <option value="patient-2">Pt 2</option>
            <option value="patient-3">Pt 3</option>
          </select>
          
          <div class="control-group">
            <select class="dropdown dark">
              <option>Simulator Mode</option>
            </select>
            <select class="dropdown dark">
              <option>Normal Profile</option>
            </select>
            <button class="btn btn-red">⏻ Disconnect</button>
          </div>
          
          <div class="ip-group" style="display:flex; align-items:center;">
             <input id="esp-ip" class="dropdown dark" style="width:130px;" type="text" placeholder="ESP32 IP" />
          </div>

          <div class="live-indicator"><span class="icon-pulse">~</span> LIVE</div>
          <button id="mode-toggle" class="btn btn-green active">
             <span class="dot green-dot"></span> <span id="mode-label">SYSTEM ACTIVE</span>
          </button>
        </div>
      </header>

      <!-- METRICS ROW -->
      <div class="metrics-row">
        
        <!-- HR -->
        <div class="metric-card">
          <div class="mc-icon green">♡</div>
          <div class="mc-value green" id="v-hr">—</div>
          <div class="mc-normal">Normal: 60-100 bpm</div>
          <div class="mc-label">HEART RATE<br><span>bpm</span></div>
        </div>
        
        <!-- HRV -->
        <div class="metric-card">
          <div class="mc-icon purple">∿</div>
          <div class="mc-value purple" id="v-hrv">—</div>
          <div class="mc-normal">Normal: 50-100 ms</div>
          <div class="mc-label">HRV (SDNN)<br><span>ms</span></div>
        </div>

        <!-- TEMP GRADIENT -->
        <div class="metric-card">
          <div class="mc-icon blue">🌡</div>
          <div class="mc-value blue" id="v-temp">—</div>
          <div class="mc-normal">Normal: &lt; 1.0 °C</div>
          <div class="mc-label">TEMP GRADIENT<br><span>°C</span></div>
        </div>



        <!-- PTT -->
        <div class="metric-card">
          <div class="mc-icon yellow">🕒</div>
          <div class="mc-value yellow" id="v-ptt">—</div>
          <div class="mc-normal">Normal: 200-300 ms</div>
          <div class="mc-label">PTT<br><span>ms</span></div>
        </div>

        <!-- BIO -->
        <div class="metric-card">
          <div class="mc-icon blue2">≈</div>
          <div class="mc-value blue2" id="v-bio">—</div>
          <div class="mc-normal">(NORMAL: 24Ω)</div>
          <div class="mc-label">BIOIMPEDANCE<br><span>Ω</span></div>
        </div>

      </div>

      <!-- WAVEFORMS -->
      <div class="waveforms-row">
        <!-- ECG -->
        <div class="waveform-card">
          <div class="wf-header">
            <div class="wf-title green">▼ ECG Waveform</div>
            <div class="wf-meta">500 samples</div>
          </div>
          <div class="chart-container">
            <canvas id="ecg-chart"></canvas>
          </div>
        </div>
        
        <!-- PPG -->
        <div class="waveform-card">
          <div class="wf-header">
            <div class="wf-title blue">💧 PPG Waveform</div>
            <div class="wf-meta">500 samples</div>
          </div>
          <div class="chart-container">
            <canvas id="ppg-chart"></canvas>
          </div>
        </div>
      </div>

      <!-- AI CLINICAL INTERPRETATION Engine -->
      <div class="ai-clinical-card" id="ai-clinical-card">
        <div class="ai-header">
           <div class="ai-title"><span class="icon">🧠</span> AI Clinical Interpretation Engine</div>
           <div class="ai-confidence">Prediction Confidence: <span id="ai-conf-value">--%</span></div>
        </div>
        <div class="ai-body">
           <div class="ai-reasons" id="ai-reasons-list">
           </div>
           <div class="ai-summary-box stable" id="ai-summary-box">
             <div class="ai-sys-label">SYSTEM INTERPRETATION SUMMARY</div>
             <div class="ai-sys-text stable" id="ai-sys-text">Awaiting data stream...</div>
           </div>
        </div>
      </div>

      <!-- ALERT BANNER -->
      <div class="alert-banner" id="alert-banner">
        <div class="alert-icon" id="alert-icon">⚠</div>
        <div class="alert-title yellow" id="alert-title">LOW RISK</div>
        <div class="alert-desc">
          <span class="alert-bolt">⚡</span> 
          <span id="alert-desc">LOW RISK — Some parameters are outside normal range. Continue monitoring.</span>
        </div>
      </div>

      </div><!-- /view-dashboard -->

      <!-- ═══════ HOSPITAL OVERVIEW VIEW ═══════ -->
      <div id="view-patients" class="app-view" style="display: none;">
        <header class="top-bar">
          <div class="top-title">
            <span class="icon-shield">👥</span> Hospital Overview
          </div>
          <div class="top-controls">
            <span class="control-label">MULTI-PATIENT MONITORING</span>
            <div class="live-indicator"><span class="icon-pulse">~</span> LIVE</div>
          </div>
        </header>

        <div class="patients-grid" id="patients-grid">
          <!-- Populated by JS -->
        </div>
      </div><!-- /view-patients -->

      <!-- ═══════ AI ANALYSIS VIEW ═══════ -->
      <div id="view-ai" class="app-view" style="display: none;">
        
        <!-- HEADER -->
        <header class="top-bar">
          <div class="top-title">
            <span class="icon-shield">🧠</span> AI Clinical Analysis Engine
            <div style="font-size: 0.75rem; color: var(--text-muted); font-weight: 500; letter-spacing: 0.05em; margin-top: 2px;">MULTIMODAL HEMODYNAMIC INTERPRETATION</div>
          </div>
          <div class="top-controls">
            <span class="control-label">ANALYSIS STREAM</span>
            <div class="live-indicator"><span class="icon-pulse">~</span> <span id="ai-live-label">LIVE</span></div>
            <div id="ai-timestamp" style="font-family: 'JetBrains Mono', monospace; font-size: 0.95rem; margin-left:15px; color: var(--text-secondary); background: var(--bg-input); padding: 4px 10px; border-radius: 4px;">--:--:--</div>
          </div>
        </header>

        <!-- TOP: VITALS ROW (Miniaturized) -->
        <div class="metrics-row ai-metrics-row" style="margin-bottom: 20px;">
          <div class="metric-card"><div class="mc-value green" id="ai-v-hr">—</div><div class="mc-label">HEART RATE</div></div>
          <div class="metric-card"><div class="mc-value purple" id="ai-v-hrv">—</div><div class="mc-label">HRV</div></div>
          <div class="metric-card"><div class="mc-value blue" id="ai-v-temp">—</div><div class="mc-label">TEMP GRADIENT</div></div>

          <div class="metric-card"><div class="mc-value yellow" id="ai-v-ptt">—</div><div class="mc-label">PTT</div></div>
          <div class="metric-card"><div class="mc-value blue2" id="ai-v-bio">—</div><div class="mc-label">BIOIMPEDANCE</div></div>
        </div>

        <!-- MIDDLE: AI MAIN LAYOUT GRID -->
        <div class="ai-layout-grid">
          
          <!-- LEFT COL: Signals & Explainability -->
          <div class="ai-col-left">
            <!-- Charts -->
            <div class="waveform-card">
               <div class="wf-header"><div class="wf-title green">▼ Fast ECG Signal Check</div><div class="wf-meta" id="ai-ecg-sqi">SQI: 100%</div></div>
               <div class="chart-container" style="height:120px;"><canvas id="ai-ecg-chart"></canvas></div>
            </div>
            <div class="waveform-card">
               <div class="wf-header"><div class="wf-title blue">💧 PPG Morphology</div><div class="wf-meta" id="ai-ppg-sqi">SQI: 100%</div></div>
               <div class="chart-container" style="height:120px;"><canvas id="ai-ppg-chart"></canvas></div>
            </div>
            
            <!-- AI Contribution -->
            <div class="ai-clinical-card" style="margin-bottom:0; flex:1;">
               <div class="ai-header" style="margin-bottom:12px; padding-bottom:8px;"><div class="ai-title">🔎 AI Contribution Extractor</div></div>
               <div class="ai-body" style="flex-direction:column; gap:10px;">
                  <div id="ai-contrib-list" style="display:flex; flex-direction:column; gap:8px;">
                     <!-- populated dynamically -->
                  </div>
               </div>
            </div>
          </div>
          
          <!-- RIGHT COL: Score, Decision, Trends, Advanced -->
          <div class="ai-col-right">
             <!-- Top Row: Score & Decision -->
             <div class="ai-top-panels">
               <div class="ai-score-panel" id="ai-score-box">
                  <div class="ai-score-label">HEMODYNAMIC INSTABILITY (HIS)</div>
                  <div class="ai-score-val-wrap"><span class="ai-score-val" id="ai-his-val">0</span><span class="ai-score-max">/100</span></div>
                  <div class="ai-score-meta">
                    <span id="ai-his-trend">→ STABLE</span>
                    <span id="ai-his-conf">CONF: 99%</span>
                  </div>
               </div>
               
               <div class="ai-decision-panel" id="ai-decision-box">
                  <div class="ai-decision-status" id="ai-decision-status">STABLE</div>
                  <div class="ai-decision-action" id="ai-decision-action">Baseline configuration tracking active.</div>
                  <div class="ai-decision-time" id="ai-decision-time">Estimated progression to critical: N/A</div>
               </div>
             </div>
             
             <!-- Middle: Interpretations -->
             <div class="ai-clinical-card">
               <div class="ai-header" style="margin-bottom:12px; padding-bottom:8px;"><div class="ai-title"><span class="icon">⚕</span> Clinical Interpretation Matrix</div></div>
               <div class="ai-reasons" id="ai-interp-list" style="display:flex; flex-direction:column; gap:8px;"></div>
             </div>

             <!-- Bot: Trend History -->
             <div class="waveform-card" style="margin-bottom:0; flex:1; display:flex; flex-direction:column;">
                <div class="wf-header">
                  <div class="wf-title">📉 HIS Trend History (Raw vs Filtered)</div>
                  <label class="adv-toggle" style="cursor:pointer; font-size:0.8rem; color:var(--text-secondary); display:flex; align-items:center; gap:6px;">
                     <input type="checkbox" id="ai-adv-toggle"> Advanced Mode
                  </label>
                </div>
                <!-- Dual pane: Chart or Adv -->
                <div class="chart-container" id="ai-trend-container" style="flex:1; min-height:160px;">
                   <canvas id="ai-trend-chart"></canvas>
                </div>
                
                <!-- Advanced Data Pane (Hidden by default) -->
                <div id="ai-advanced-panel" class="ai-adv-panel" style="display:none; flex:1; min-height:160px; grid-template-columns: repeat(3, 1fr); gap: 10px;">
                   <div class="adv-stat"><div class="adv-lbl">ΔHR/dt (bpm/s)</div><div class="adv-val" id="adv-hr-sl">—</div></div>
                   <div class="adv-stat"><div class="adv-lbl">ΔPTT/dt (ms/s)</div><div class="adv-val" id="adv-ptt-sl">—</div></div>
                   <div class="adv-stat"><div class="adv-lbl">ΔBio/dt (Ω/s)</div><div class="adv-val" id="adv-bio-sl">—</div></div>
                   <div class="adv-stat"><div class="adv-lbl">HR StDev (σ)</div><div class="adv-val" id="adv-hr-var">—</div></div>
                   <div class="adv-stat"><div class="adv-lbl">Time Window</div><div class="adv-val">10.0s</div></div>
                   <div class="adv-stat"><div class="adv-lbl">Base Locked</div><div class="adv-val green" id="adv-base-lock">Calibrating...</div></div>
                </div>
             </div>
          </div>
          
        </div>

      </div><!-- /view-ai -->

    </main>
  </div><!-- /dashboard-wrapper -->

  <script src="script2.js"></script>
</body>
</html>)rawliteral";

#endif
