#ifndef PAGE_CSS_H
#define PAGE_CSS_H

const char PAGE_CSS[] PROGMEM = R"rawliteral(
/* ============================================================
   HEMOGUARD — Real-Time Biomedical Monitoring Dashboard
   Dark medical theme  ·  Glassmorphism cards  ·  CSS Grid
   ============================================================ */

   @import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700;800&family=JetBrains+Mono:wght@400;500&display=swap');

   /* ---------- design tokens ---------- */
   :root {
     --bg-base:       #0d1117; /* Very dark blue/black behind everything */
     --bg-sidebar:    #121826; /* Dark sidebar */
     --bg-card:       #1c2333; /* Dark blue cards */
     --bg-input:      #1a202c; /* Input dark */
     --border-color:  rgba(255,255,255,0.05);
     --border-card:   #2a354d;

     --text-primary:   #e2e8f0;
     --text-secondary: #94a3b8;
     --text-muted:     #64748b;
   
     --accent-green:   #10b981;
     --accent-purple:  #b48ead;
     --accent-blue:    #38bdf8;
     --accent-red:     #f43f5e;
     --accent-yellow:  #fbbf24;
     --accent-blue2:   #60a5fa;

     --radius:   12px;
     --radius-sm: 8px;
     --transition: .25s ease;
   }
   
   /* ---------- reset ---------- */
   *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
   html { font-size: 15px; }
   
   body {
     font-family: 'Inter', system-ui, sans-serif;
     background: var(--bg-base);
     color: var(--text-primary);
     min-height: 100vh;
     overflow-x: hidden;
   }
   
   /* ---------- layout ---------- */
   .dashboard-wrapper {
     display: flex;
     width: 100%;
     height: 100vh;
     overflow: hidden;
   }
   
   /* ---------- sidebar ---------- */
   .sidebar {
     width: 250px;
     background: var(--bg-sidebar);
     border-right: 1px solid var(--border-color);
     display: flex;
     flex-direction: column;
     padding: 24px 0;
     flex-shrink: 0;
   }

   .sidebar-brand {
     padding: 0 24px 30px;
   }
   .brand-title {
     font-size: 1.35rem;
     font-weight: 700;
     color: var(--accent-green);
   }
   .brand-subtitle {
     font-size: 0.65rem;
     font-weight: 600;
     letter-spacing: 0.08em;
     color: var(--text-muted);
     text-transform: uppercase;
     margin-top: 4px;
   }
   
   .nav-section {
     margin-bottom: 24px;
   }
   .nav-header {
     padding: 0 24px;
     font-size: 0.68rem;
     font-weight: 700;
     letter-spacing: 0.1em;
     color: var(--text-muted);
     margin-bottom: 12px;
   }
   .nav-item {
     display: flex;
     align-items: center;
     gap: 12px;
     padding: 10px 24px;
     color: var(--text-secondary);
     text-decoration: none;
     font-size: 0.9rem;
     font-weight: 500;
     transition: var(--transition);
   }
   .nav-item:hover {
     background: rgba(255,255,255,0.03);
     color: var(--text-primary);
   }
   .nav-item.active {
     background: rgba(16, 185, 129, 0.1);
     color: var(--accent-green);
     border-left: 3px solid var(--accent-green);
   }
   .nav-icon {
     font-size: 1.1rem;
   }

   .sidebar-footer {
     margin-top: auto;
   }
   .sys-status {
     padding: 0 24px;
     display: flex;
     align-items: center;
     gap: 8px;
     font-size: 0.85rem;
     font-weight: 600;
   }
   .sys-status.online { color: var(--accent-green); }

   /* ---------- main content ---------- */
   .main-content {
     flex: 1;
     display: flex;
     flex-direction: column;
     padding: 24px 32px;
     overflow-y: auto;
     background: var(--bg-base);
   }
   
   /* ---------- top bar ---------- */
   .top-bar {
     display: flex;
     align-items: center;
     justify-content: space-between;
     margin-bottom: 26px;
     flex-wrap: wrap;
     gap: 16px;
   }
   .top-controls {
     display: flex;
     align-items: center;
     gap: 12px;
     flex-wrap: wrap;     
   }
   .top-title {
     font-size: 1.1rem;
     font-weight: 600;
     display: flex;
     align-items: center;
     gap: 10px;
   }

   /* Controls */
   .control-label {
      font-size: 0.75rem;
      font-weight: 600;
      color: var(--text-muted);
      letter-spacing: 0.05em;
   }
   .dropdown {
     background: var(--bg-input);
     color: var(--text-primary);
     border: 1px solid var(--border-card);
     padding: 8px 16px;
     border-radius: var(--radius-sm);
     font-size: 0.82rem;
     font-weight: 500;
     outline: none;
     font-family: 'Inter', sans-serif;
     cursor: pointer;
   }
   .dropdown.dark { background: var(--bg-card); }
   .control-group {
      display: flex;
      gap: 6px;
   }

   .btn {
     padding: 8px 16px;
     border-radius: var(--radius-sm);
     border: 1px solid transparent;
     font-size: 0.82rem;
     font-weight: 600;
     cursor: pointer;
     transition: var(--transition);
     display: inline-flex;
     align-items: center;
     gap: 6px;
   }
   .btn-red {
     background: rgba(244, 63, 94, 0.1);
     color: var(--accent-red);
     border-color: rgba(244, 63, 94, 0.2);
   }
   .btn-green {
     background: rgba(16, 185, 129, 0.1);
     color: var(--accent-green);
     border-color: rgba(16, 185, 129, 0.4);
     border-radius: 20px;
     padding: 6px 14px;
   }
   .live-indicator {
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 0.8rem;
      font-weight: 700;
      color: var(--accent-blue);
      letter-spacing: 0.05em;
      margin: 0 8px;
   }
   
   /* ---------- metrics row ---------- */
   .metrics-row {
     display: grid;
     grid-template-columns: repeat(6, 1fr);
     gap: 16px;
     margin-bottom: 24px;
   }
   
   .metric-card {
     background: var(--bg-card);
     border-radius: var(--radius);
     padding: 24px 16px;
     text-align: center;
     display: flex;
     flex-direction: column;
     align-items: center;
     justify-content: center;
     box-shadow: 0 4px 20px rgba(0,0,0,0.15);
     border: 1px solid transparent;
   }

   .mc-icon {
     font-size: 1.2rem;
     margin-bottom: 12px;
   }
   .mc-value {
     font-family: 'JetBrains Mono', monospace;
     font-size: 2.2rem;
     font-weight: 700;
     line-height: 1;
     margin-bottom: 8px;
   }
   .mc-normal {
     font-size: 0.65rem;
     color: var(--text-muted);
     margin-bottom: 8px;
   }
   .mc-label {
     font-size: 0.72rem;
     font-weight: 600;
     letter-spacing: 0.06em;
     color: var(--text-secondary);
     line-height: 1.4;
   }
   .mc-label span {
     font-size: 0.65rem;
     color: var(--text-muted);
     text-transform: lowercase;
   }
   
   /* colors mapping for cards */
   .green  { color: var(--accent-green); }
   .purple { color: var(--accent-purple); }
   .blue   { color: var(--accent-blue); }
   .red    { color: var(--accent-red); }
   .yellow { color: var(--accent-yellow); }
   .blue2  { color: var(--accent-blue2); }
   
   /* ---------- waveforms row ---------- */
   .waveforms-row {
     display: grid;
     grid-template-columns: 1fr 1fr;
     gap: 20px;
     margin-bottom: 24px;
   }
   
   .waveform-card {
     background: var(--bg-card);
     border-radius: var(--radius);
     padding: 20px;
     box-shadow: 0 4px 20px rgba(0,0,0,0.15);
   }
   
   .wf-header {
     display: flex;
     justify-content: space-between;
     align-items: center;
     margin-bottom: 16px;
   }
   .wf-title {
     font-size: 0.85rem;
     font-weight: 600;
   }
   .wf-meta {
     font-size: 0.7rem;
     color: var(--text-muted);
     font-family: 'JetBrains Mono', monospace;
   }

   .chart-container {
     position: relative;
     width: 100%;
     height: 160px;
   }
   
   /* ---------- AI Clinical Interpretation ---------- */
   .ai-clinical-card {
     background: var(--bg-card);
     border-radius: var(--radius);
     padding: 24px;
     margin-bottom: 24px;
     box-shadow: 0 4px 20px rgba(0,0,0,0.15);
     border: 1px solid transparent;
   }
   
   .ai-clinical-card.pulse-anim {
     animation: aiPulse 0.6s ease-out;
   }
   
   @keyframes aiPulse {
     0% { box-shadow: 0 0 5px rgba(255,255,255,0.05); }
     30% { box-shadow: 0 0 20px rgba(255,255,255,0.15); }
     100% { box-shadow: 0 4px 20px rgba(0,0,0,0.15); }
   }
   
   .ai-header {
     display: flex;
     justify-content: space-between;
     align-items: center;
     margin-bottom: 20px;
     padding-bottom: 14px;
     border-bottom: 1px solid var(--border-color);
   }
   .ai-title {
     font-size: 1.1rem;
     font-weight: 700;
     letter-spacing: 0.02em;
     display: flex;
     align-items: center;
     gap: 8px;
   }
   .ai-confidence {
     font-size: 0.85rem;
     font-weight: 600;
     color: var(--text-secondary);
   }
   #ai-conf-value {
     font-family: 'JetBrains Mono', monospace;
     color: var(--accent-blue);
     font-size: 1.1rem;
     margin-left: 6px;
   }
   
   .ai-body {
     display: flex;
     gap: 30px;
     align-items: stretch;
   }
   
   .ai-reasons {
     flex: 3;
     display: flex;
     flex-direction: column;
     gap: 12px;
     justify-content: center;
   }
   .ai-reason-item {
     font-size: 0.88rem;
     color: var(--text-primary);
     display: flex;
     align-items: center;
     gap: 10px;
   }
   .ai-reason-item span {
     color: var(--text-muted);
     font-size: 1.1rem;
   }
   .ai-reason-item strong {
     color: var(--text-primary);
     font-weight: 600;
   }
   
   .ai-reason-item.stable strong   { color: var(--accent-green); }
   .ai-reason-item.warning strong  { color: var(--accent-yellow); }
   .ai-reason-item.critical strong { color: var(--accent-red); }
   
   .ai-summary-box {
     flex: 2;
     background: var(--bg-input);
     border-radius: var(--radius-sm);
     padding: 24px;
     display: flex;
     flex-direction: column;
     justify-content: center;
     border-left: 5px solid var(--text-muted);
     transition: border-color 0.3s ease;
   }
   .ai-summary-box.stable   { border-color: var(--accent-green); box-shadow: inset 20px 0 30px -20px rgba(16, 185, 129, 0.1); }
   .ai-summary-box.warning  { border-color: var(--accent-yellow); box-shadow: inset 20px 0 30px -20px rgba(251, 191, 36, 0.1); }
   .ai-summary-box.critical { border-color: var(--accent-red); box-shadow: inset 20px 0 30px -20px rgba(244, 63, 94, 0.1); }
   
   .ai-sys-label {
     font-size: 0.72rem;
     font-weight: 700;
     letter-spacing: 0.1em;
     color: var(--text-muted);
     margin-bottom: 12px;
   }
   .ai-sys-text {
     font-size: 1rem;
     font-weight: 500;
     line-height: 1.5;
   }
   .ai-sys-text.stable   { color: var(--accent-green); }
   .ai-sys-text.warning  { color: var(--accent-yellow); }
   .ai-sys-text.critical { color: var(--accent-red); }
   
   /* ---------- alert banner ---------- */
   .alert-banner {
     background: var(--bg-card);
     border-radius: var(--radius);
     padding: 24px;
     text-align: center;
     box-shadow: 0 4px 20px rgba(0,0,0,0.15);
     display: flex;
     flex-direction: column;
     align-items: center;
     justify-content: center;
     border: 1px solid transparent;
   }

   .alert-icon {
     font-size: 2rem;
     margin-bottom: 8px;
     color: var(--accent-yellow);
   }

   .alert-title {
     font-size: 1.4rem;
     font-weight: 800;
     letter-spacing: 0.04em;
     margin-bottom: 12px;
   }

   .alert-desc {
     font-size: 0.85rem;
     color: var(--text-secondary);
     display: flex;
     align-items: center;
     gap: 8px;
   }

   /* state colors for alert banner */
   .alert-banner.stable .alert-icon { color: var(--accent-green); }
   .alert-banner.stable .alert-title { color: var(--accent-green); }
   
   .alert-banner.warning .alert-icon { color: var(--accent-yellow); }
   .alert-banner.warning .alert-title { color: var(--accent-yellow); }

   .alert-banner.critical .alert-icon { color: var(--accent-red); }
   .alert-banner.critical .alert-title { color: var(--accent-red); }
   .alert-banner.critical { animation: alertPulse 1.5s infinite; }

   @keyframes alertPulse {
     0% { box-shadow: 0 0 0 rgba(244, 63, 94, 0); border-color: transparent;}
     50% { box-shadow: 0 0 30px rgba(244, 63, 94, 0.2); border-color: rgba(244,63,94,0.3);}
     100% { box-shadow: 0 0 0 rgba(244, 63, 94, 0); border-color: transparent;}
   }

   .dot {
     display: inline-block;
     width: 8px; height: 8px;
     border-radius: 50%;
   }
   .green-dot { background: var(--accent-green); box-shadow: 0 0 8px var(--accent-green); }
   
   /* ---------- responsive ---------- */
   @media (max-width: 1400px) {
     .metrics-row {
       grid-template-columns: repeat(3, 1fr);
     }
   }
   @media (max-width: 1000px) {
     .dashboard-wrapper { flex-direction: column; }
     .sidebar { width: 100%; height: auto; padding: 16px; border-right: none; border-bottom: 1px solid var(--border-color); }
     .waveforms-row { grid-template-columns: 1fr; }
   }

   /* ---------- patients overview grid ---------- */
   .app-view {
     animation: fadeIn 0.3s ease;
   }
   @keyframes fadeIn {
     from { opacity: 0; transform: translateY(5px); }
     to { opacity: 1; transform: translateY(0); }
   }
   
   .patients-grid {
     display: flex;
     flex-direction: column;
     gap: 16px;
     width: 100%;
   }
   
   .patient-card {
     background: var(--bg-card);
     border-radius: var(--radius);
     padding: 24px;
     display: flex;
     align-items: center;
     justify-content: space-between;
     box-shadow: 0 4px 20px rgba(0,0,0,0.15);
     border: 1px solid var(--border-color);
     transition: transform 0.2s ease, box-shadow 0.2s ease;
     cursor: pointer;
     width: 100%;
   }
   .patient-card:hover {
     transform: translateY(-2px);
     box-shadow: 0 8px 25px rgba(0,0,0,0.25);
     background: var(--bg-input);
     border-color: rgba(255,255,255,0.1);
   }
   
   .patient-card.critical {
     border-color: rgba(244, 63, 94, 0.4);
     box-shadow: 0 0 15px rgba(244, 63, 94, 0.15);
     animation: redGlow 2s infinite;
   }
   @keyframes redGlow {
     0% { box-shadow: 0 0 10px rgba(244, 63, 94, 0.1); }
     50% { box-shadow: 0 0 30px rgba(244, 63, 94, 0.3); border-color: rgba(244, 63, 94, 0.8);}
     100% { box-shadow: 0 0 10px rgba(244, 63, 94, 0.1); }
   }
   
   .pc-info {
     flex: 1.2;
   }
   .pc-name {
     font-size: 1.25rem;
     font-weight: 700;
     margin-bottom: 8px;
     display: flex;
     align-items: center;
     gap: 8px;
   }
   .pc-status {
     font-size: 0.75rem;
     font-weight: 700;
     letter-spacing: 0.08em;
     padding: 4px 10px;
     border-radius: 4px;
     display: inline-block;
     text-transform: uppercase;
   }
   .pc-status.stable { background: rgba(16,185,129,0.1); color: var(--accent-green); }
   .pc-status.warning { background: rgba(251,191,36,0.1); color: var(--accent-yellow); }
   .pc-status.critical { background: rgba(244,63,94,0.1); color: var(--accent-red); }
   
   .pc-vitals {
     flex: 1.5;
     display: flex;
     gap: 32px;
     justify-content: center;
   }
   .pc-vital {
     text-align: center;
   }
   .pc-vital-val {
     font-family: 'JetBrains Mono', monospace;
     font-size: 1.6rem;
     font-weight: 700;
   }
   .pc-vital-val.stable { color: var(--text-primary); }
   .pc-vital-val.warning { color: var(--accent-yellow); }
   .pc-vital-val.critical { color: var(--accent-red); }
   .pc-vital-lbl {
     font-size: 0.7rem;
     color: var(--text-muted);
     margin-top: 4px;
   }
   
   .pc-chart {
     flex: 1;
     height: 60px;
     max-width: 250px;
     position: relative;
   }

   /* ---------- AI ANALYSIS VIEW ---------- */
   .ai-layout-grid {
     display: flex;
     gap: 24px;
     align-items: stretch;
   }
   .ai-col-left {
     flex: 1;
     display: flex;
     flex-direction: column;
     gap: 20px;
   }
   .ai-col-right {
     flex: 2;
     display: flex;
     flex-direction: column;
     gap: 20px;
   }
   
   .ai-metrics-row {
     display: grid;
     grid-template-columns: repeat(6, 1fr);
     gap: 16px;
   }
   
   .ai-top-panels {
     display: flex;
     gap: 20px;
   }
   
   .ai-score-panel {
     flex: 1;
     background: var(--bg-card);
     border-radius: var(--radius);
     padding: 24px;
     display: flex;
     flex-direction: column;
     justify-content: center;
     align-items: center;
     border: 1px solid var(--border-color);
     box-shadow: 0 4px 20px rgba(0,0,0,0.15);
   }
   .ai-score-label {
     font-size: 0.75rem;
     font-weight: 700;
     color: var(--text-muted);
     letter-spacing: 0.05em;
     margin-bottom: 12px;
     text-align: center;
   }
   .ai-score-val-wrap {
     display: flex;
     align-items: baseline;
     gap: 4px;
   }
   .ai-score-val {
     font-size: 4.5rem;
     font-weight: 800;
     line-height: 1;
     font-family: 'JetBrains Mono', monospace;
   }
   .ai-score-val.stable { color: var(--accent-green); text-shadow: 0 0 20px rgba(16,185,129,0.3); }
   .ai-score-val.warning { color: var(--accent-yellow); text-shadow: 0 0 20px rgba(251,191,36,0.3); }
   .ai-score-val.critical { color: var(--accent-red); text-shadow: 0 0 20px rgba(244,63,94,0.3); }

   .ai-score-max {
     font-size: 1.4rem;
     color: var(--text-muted);
     font-weight: 600;
   }
   .ai-score-meta {
     margin-top: 16px;
     display: flex;
     justify-content: space-between;
     width: 100%;
     font-size: 0.8rem;
     font-weight: 700;
     color: var(--text-secondary);
   }
   
   .ai-decision-panel {
     flex: 1.5;
     background: var(--bg-card);
     border-radius: var(--radius);
     padding: 30px;
     display: flex;
     flex-direction: column;
     justify-content: center;
     border-left: 6px solid var(--border-color);
     transition: all 0.3s ease;
     box-shadow: 0 4px 20px rgba(0,0,0,0.15);
   }
   .ai-decision-panel.stable { border-color: var(--accent-green); box-shadow: inset 40px 0 60px -40px rgba(16,185,129,0.15); }
   .ai-decision-panel.warning { border-color: var(--accent-yellow); box-shadow: inset 40px 0 60px -40px rgba(251,191,36,0.15); }
   .ai-decision-panel.critical { border-color: var(--accent-red); box-shadow: inset 40px 0 60px -40px rgba(244,63,94,0.2); animation: redGlow 1.5s infinite; }
   
   .ai-decision-status {
     font-size: 3rem;
     font-weight: 800;
     line-height: 1;
     margin-bottom: 12px;
     letter-spacing: 0.02em;
   }
   .ai-decision-panel.stable .ai-decision-status { color: var(--accent-green); }
   .ai-decision-panel.warning .ai-decision-status { color: var(--accent-yellow); }
   .ai-decision-panel.critical .ai-decision-status { color: var(--accent-red); }
   
   .ai-decision-action {
     font-size: 1.15rem;
     font-weight: 500;
     color: var(--text-primary);
     margin-bottom: 12px;
   }
   .ai-decision-time {
     font-size: 0.85rem;
     color: var(--text-muted);
     font-family: 'JetBrains Mono', monospace;
   }
   
   .ai-adv-panel {
     background: var(--bg-input);
     padding: 20px;
     border-radius: var(--radius-sm);
   }
   .adv-stat {
     display: flex;
     flex-direction: column;
     gap: 6px;
   }
   .adv-lbl {
     font-size: 0.75rem;
     color: var(--text-muted);
     font-weight: 700;
     letter-spacing: 0.05em;
   }
   .adv-val {
     font-family: 'JetBrains Mono', monospace;
     font-size: 1.1rem;
     color: var(--text-primary);
   }
   
   .contrib-item {
     display: flex;
     justify-content: space-between;
     align-items: center;
     padding: 10px 14px;
     background: rgba(255,255,255,0.03);
     border-radius: 6px;
     font-size: 0.9rem;
   }
   .contrib-item div:first-child { color: var(--text-secondary); font-weight: 500;}
   .contrib-item div:last-child { font-family: 'JetBrains Mono', monospace; font-weight: 600; }
   
   @media (max-width: 1200px) {
     .ai-layout-grid { flex-direction: column; }
     .ai-metrics-row { grid-template-columns: repeat(3, 1fr); }
     .ai-top-panels { flex-direction: column; }
   })rawliteral";

#endif
