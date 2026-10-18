#pragma once
#include <Arduino.h>

const char DASHBOARD[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>iYum Water Automation</title>
<style>
:root{
  --bg:#090d16;
  --card:#131c2e;
  --card-border:rgba(255,255,255,0.08);
  --accent:#00c6ff;
  --accent2:#0072ff;
  --accent-glow:rgba(0,198,255,0.35);
  --success:#10b981;
  --warn:#f59e0b;
  --danger:#ef4444;
  --text:#e2e8f0;
  --text-muted:#8094ae;
}
*{box-sizing:border-box;margin:0;padding:0}
body{
  font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;
  background:linear-gradient(135deg,#070b12 0%,#0f172a 100%);
  color:var(--text);
  min-height:100vh;
  padding:12px 10px 32px;
  -webkit-tap-highlight-color:transparent;
}
.container{width:100%;max-width:760px;margin:0 auto}

/* Mobile-First Header */
header{
  display:flex;flex-wrap:wrap;justify-content:space-between;align-items:center;
  gap:10px;margin-bottom:14px;padding-bottom:12px;
  border-bottom:1px solid var(--card-border);
}
.logo-area{display:flex;align-items:center;gap:10px}
.logo-img{
  width:38px;height:38px;border-radius:10px;
  box-shadow:0 0 16px rgba(0,198,255,0.4);display:block;
}
h1{font-size:19px;font-weight:800;letter-spacing:-0.3px}
.header-badges{display:flex;gap:6px;flex-wrap:wrap}
.badge{
  padding:4px 9px;border-radius:14px;font-size:11px;font-weight:700;
  display:inline-flex;align-items:center;gap:5px;
  background:rgba(255,255,255,0.05);border:1px solid var(--card-border);
}
.dot{width:7px;height:7px;border-radius:50%;display:inline-block}
.dot-green{background:var(--success);box-shadow:0 0 6px var(--success)}
.dot-amber{background:var(--warn);box-shadow:0 0 6px var(--warn)}
.dot-red{background:var(--danger);box-shadow:0 0 6px var(--danger)}
.dot-blue{background:var(--accent);box-shadow:0 0 6px var(--accent)}

/* Mobile-First Main View (Single column by default) */
.main-view{display:flex;flex-direction:column;gap:12px;margin-bottom:14px}
@media (min-width:620px){
  .main-view{display:grid;grid-template-columns:230px 1fr;gap:14px}
}

.tank-card{
  background:var(--card);border:1px solid var(--card-border);border-radius:16px;
  padding:16px 12px;display:flex;flex-direction:column;align-items:center;
  justify-content:center;box-shadow:0 4px 20px rgba(0,0,0,0.3);
}

/* Tank Graphic with Guaranteed Marker Visibility */
.tank-wrapper{
  width:140px;height:220px;border-radius:24px;
  border:3px solid rgba(255,255,255,0.2);position:relative;
  background:rgba(0,0,0,0.55);overflow:hidden;box-shadow:inset 0 0 16px rgba(0,0,0,0.6);
}
.tank-scale{
  position:absolute;left:6px;top:0;bottom:0;width:30px;pointer-events:none;z-index:4;
  display:flex;flex-direction:column;justify-content:space-between;padding:12px 0;
  font-size:10px;font-weight:700;color:rgba(255,255,255,0.45);
}
.tank-water{
  position:absolute;bottom:0;left:0;right:0;height:0%;
  background:linear-gradient(180deg,#38bdf8 0%,#0284c7 50%,#0369a1 100%);
  transition:height 0.8s cubic-bezier(0.4,0,0.2,1);
  box-shadow:0 0 20px rgba(56,189,248,0.45);
  z-index:2;
}
.tank-water::before{
  content:"";position:absolute;top:-6px;left:0;right:0;height:12px;
  background:rgba(255,255,255,0.35);border-radius:50%;
}

/* Clamped Threshold Markers (Always Visible, even at 100% or 0%) */
.tank-marker{
  position:absolute;left:30px;right:0;height:0;z-index:10;pointer-events:none;
  display:flex;align-items:center;transition:bottom 0.5s ease;
}
.tank-marker .marker-line{flex:1;height:0;border-top:2px dashed;opacity:0.95}
.tank-marker .marker-tag{
  font-size:9px;font-weight:800;padding:2px 5px;border-radius:4px;
  white-space:nowrap;margin-right:4px;box-shadow:0 2px 6px rgba(0,0,0,0.6);
}
.marker-stop .marker-line{border-color:#ef4444}
.marker-stop .marker-tag{background:rgba(239,68,68,0.85);color:#fff;border:1px solid #ef4444}
.marker-start .marker-line{border-color:#10b981}
.marker-start .marker-tag{background:rgba(16,185,129,0.85);color:#fff;border:1px solid #10b981}

.tank-readout{margin-top:10px;text-align:center}
.tank-percent{font-size:34px;font-weight:800;color:#fff;line-height:1}
.tank-status-label{font-size:12px;color:var(--text-muted);font-weight:600;margin-top:4px}
.zone-badge{
  font-size:11px;font-weight:700;padding:3px 8px;border-radius:6px;
  margin-top:6px;display:inline-block;
}
.zone-fill{background:rgba(16,185,129,0.15);color:#34d399;border:1px solid rgba(16,185,129,0.35)}
.zone-hold{background:rgba(56,189,248,0.15);color:#38bdf8;border:1px solid rgba(56,189,248,0.35)}
.zone-full{background:rgba(99,102,241,0.15);color:#a5b4fc;border:1px solid rgba(99,102,241,0.35)}

/* Mobile 3-Card Stats Grid (Water Level card removed as redundant) */
.stats-grid{
  display:grid;grid-template-columns:repeat(3,1fr);gap:8px;
}
.card{
  background:var(--card);border:1px solid var(--card-border);border-radius:14px;
  padding:12px 10px;box-shadow:0 4px 16px rgba(0,0,0,0.2);
  display:flex;flex-direction:column;justify-content:center;text-align:center;
}
.card-title{
  font-size:10px;color:var(--text-muted);font-weight:700;
  text-transform:uppercase;letter-spacing:0.4px;margin-bottom:4px;
}
.card-val{font-size:17px;font-weight:800;color:#fff}

/* Mobile-First Action Buttons */
.btn-group{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:8px}
.btn{
  border:0;border-radius:10px;min-height:44px;padding:10px 8px;
  font-size:13px;font-weight:700;cursor:pointer;color:#fff;
  background:#1e293b;border:1px solid var(--card-border);
  transition:all .15s ease;
  display:flex;align-items:center;justify-content:center;text-align:center;
}
.btn:hover{transform:translateY(-1px)}
.btn:active{transform:scale(0.96)}
.btn-auto{background:linear-gradient(135deg,#2563eb,#1d4ed8)}
.btn-on{background:linear-gradient(135deg,#059669,#047857)}
.btn-off{background:linear-gradient(135deg,#dc2626,#b91c1c)}
.btn.active-mode{outline:3px solid #38bdf8;outline-offset:2px}

.toast{
  padding:8px 12px;border-radius:8px;background:rgba(255,255,255,0.05);
  border:1px solid var(--card-border);font-size:12px;color:var(--text);
  margin-top:8px;display:flex;align-items:center;gap:6px;
}
.section-title{
  font-size:14px;font-weight:700;margin:16px 0 8px;
  display:flex;align-items:center;justify-content:space-between;
}

/* Preset Sensitivity Grid (3 cols on mobile) */
.preset-grid{
  display:grid;grid-template-columns:repeat(3,1fr);gap:6px;margin-bottom:8px;
}
@media (min-width:540px){
  .preset-grid{grid-template-columns:repeat(6,1fr)}
}
.preset-btn{
  background:rgba(255,255,255,0.04);border:1px solid var(--card-border);
  color:var(--text);min-height:38px;padding:6px 4px;border-radius:8px;font-size:12px;
  font-weight:700;cursor:pointer;transition:all .15s ease;text-align:center;
}
.preset-btn:hover{background:rgba(0,198,255,0.12);border-color:var(--accent);color:#fff}
.preset-btn.active-preset{
  background:linear-gradient(135deg,rgba(0,198,255,0.25),rgba(0,114,255,0.25));
  border-color:var(--accent);color:#fff;outline:2px solid var(--accent);
}

.custom-sensitivity-row{
  display:flex;align-items:center;gap:8px;padding-top:8px;
  border-top:1px solid var(--card-border);
}
.custom-input{
  max-width:80px;padding:6px 8px;background:rgba(0,0,0,0.3);
  border:1px solid var(--card-border);border-radius:6px;color:#fff;
  font-size:13px;font-weight:700;text-align:center;
}
.custom-btn{
  padding:6px 14px;border-radius:6px;font-size:12px;font-weight:700;
  background:linear-gradient(135deg,#00c6ff,#0072ff);color:#fff;border:0;cursor:pointer;
}

.meter-wrapper{margin-top:8px}
.meter-label-row{
  display:flex;justify-content:space-between;font-size:10px;
  color:var(--text-muted);margin-bottom:3px;
}
.meter-bar{height:5px;background:rgba(255,255,255,0.06);border-radius:3px;overflow:hidden}
.meter-fill{
  height:100%;width:0%;
  background:linear-gradient(90deg,var(--accent),var(--success));
  border-radius:3px;transition:width 0.3s ease;
}

/* Mobile-First Form */
.form-grid{display:grid;grid-template-columns:1fr;gap:10px}
@media (min-width:480px){
  .form-grid{grid-template-columns:repeat(2,1fr)}
}
@media (min-width:680px){
  .form-grid{grid-template-columns:repeat(3,1fr)}
}
.form-item{display:flex;flex-direction:column}
label{font-size:12px;color:var(--text-muted);font-weight:600;margin-bottom:3px}
input{
  width:100%;min-height:40px;padding:8px 10px;background:rgba(0,0,0,0.3);
  border:1px solid var(--card-border);border-radius:8px;
  color:#fff;font-size:14px;transition:border-color .15s;
}
input:focus{outline:0;border-color:var(--accent)}
.copy-btn{
  padding:4px 8px;font-size:11px;border-radius:6px;
  background:rgba(255,255,255,0.1);color:#fff;border:0;cursor:pointer;margin-left:6px;
}
</style>
</head>
<body>
<div class="container">
<header>
  <div class="logo-area">
    <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAMAAABg3Am1AAAAwFBMVEUEwvQYjN8/Ye4Raa67QL9qQ98oQqAKOV9JJGwPJ1IMKlALKE4LJEYKJEkKIUQJI0UJIEIJH0AJHT8IHToiFDsLFTIJGTcIGjkIGTcIFzUIFjIIEy8HGDYHFjMHFTEHFC4HEy8HEiwHESsGFDAGEi0GECkFESoSDi4HDSgGDigGDiYGCyQFDicFDSQFDCUFDCQFDCIFCyEEDSIECyMECyIECx8ECiIDCh0DCBoBBQwBAwcBAgQAChwAAgUAAAEAAAAoAQJFAAAEeElEQVR42o2WCXOqWBSEZRM0LggqIEsAAZGAGhXBhKf//19NHxA1pubNdBWpMtXfPX3uwqVzvety0/Wv6tzdf/ddXgD8/v76d/25/KpQqm990ttwOBiP5ImiqJo2hwxjsTBdf3/+AXyrotjvt8RwMAJQE9rcmhv2wlw6Zn65A5eLfLfDP64rqAr8+lynCo757nnO/nIDLteF2NoBjEdtIkjXyQ/A9Ty3pFY7+HPu//YrKhEoYMPvvAPwQ//r2gDv4s2O+ARMlInlmT41jUCOaTomCgCgEgRMHuNTA/AHA44RPVslP41PBfwwLG5Nj+79ApAnEyUWOxCn+hoFWjrUgR+FYX4DJvXorV/WPQ5ukekw/cwIMT9JgPH9cP0CDJs8I2cCv/jxqXIdRsy7DMN2wzrQC9D0K8s+xRFPuuZmMHPws+w89QhYt4DSpKHhFRN+Ro1VTdeMQmSAnDl2lnrwJ0kLqG+DJv7I0in+xlXj0+nk6p8WB6ALwP8JYPTBWFZks88gTmG5schx3OTTCPOuWBKAPOskfQAjWt4JxWfGJ80ZIomIlUhtt8hyAJVHwL0HHYA80lyKE601Lea4MceJfc4N3KVTdFl2VPnrJHsAY1keGfVsnhzNMKzYZroiI57dIPCD0AchVus0fQCYTa2ZTU23DMuw1VDkPqzAdQOsWFaKLNYiTdJ2a1gjbE7EsU+WbdtLUiFyYRpFUZiSzj2W7aFGC9iyoi447uRNVGhey98FwayRac7KWS/ZbTd3AOdLiz/qNWYaYXl7GksSSGZe7ra7rAUcOl6qWgOd1s8O561dEPws3m539wrvSn0aLSvhOpwSBZGDeVmvsxkrlDNBKOMky7akF0ALBh0my4MwyrAdekVKZjxFnBGw+zyUz4Cua8jEne2l69rYDmIVwnw28VT7FMDh+AAwumZZlqMzTG8XBuFeweLmVKEnCYIkTY+b7e74eXwCIKzYQaTjskcHQrf0s1ndMM/zUhkfDq+AZRi2s+9xLIs8AjoICZAk2KVpsUGiH5Gs2m8vd122e0aMfO/XwJ+Y57/OFSU6PjWNDQe/bUfvHNstw7IrCPsqzWY8X0zxxMl2R8CpBTydNpxtO3P4CxzHQuAFaboHUAGo4KcWTg9gTgUcI0OefRr43iEWqNPVDdgQgDP7DNh2vU+7zj7w02h2zCRptfclCc803+6wCs9AVAN4xwVF5HqRiZlBgSpOq2qdlRUWufE/AHpHL2xn6TpuYB4kviamxxhvuzTGtsOcnvInIDEWNvwAcCbNjcSveGnKT6s4wbncbBoAuk3r5VoaDt0aDh011wym0hSSVnGcxAkBH7dEm3P9ur9evxYOFQCAIqa7mtYCsCb/5kA6YaEPX+2VVRhOU8CFvNkKyGq1ihEozVChJo7HTfm4FDObLg2Xegg8E0Tth7JNhgrHxn95XLuXvG6Z/Hiv0G3g+wBS1MiaAttDeflxsZ/zkK6NEPcM3qMxhHNJwiaC8vL88iWAKn9afb8I//r96XD9j4+T38D/1T846EXsqF6RiQAAAABJRU5ErkJggg==" class="logo-img" width="38" height="38" alt="iYum">
    <div>
      <h1>iYum Water</h1>
      <div style="font-size:11px;color:var(--text-muted)" id="dev-info">Connecting...</div>
    </div>
  </div>
  <div class="header-badges">
    <div class="badge"><span class="dot dot-green"></span><span id="badge-controller">Online</span></div>
    <div class="badge"><span class="dot" id="dot-relay"></span><span id="badge-relay">Switch...</span></div>
    <div class="badge" style="background:rgba(0,198,255,0.08);border-color:rgba(0,198,255,0.3)">
      <span class="dot dot-blue"></span>
      <a href="http://iyum-water.local" style="color:var(--accent);text-decoration:none;font-weight:700">http://iyum-water.local</a>
    </div>
  </div>
</header>

<div class="main-view">
  <!-- Graphical Water Tank -->
  <div class="tank-card">
    <div class="tank-wrapper">
      <div class="tank-scale">
        <span>100%</span>
        <span>75%</span>
        <span>50%</span>
        <span>25%</span>
        <span>0%</span>
      </div>
      <!-- Stop Line (Position clamped to always stay visible inside tank) -->
      <div class="tank-marker marker-stop" id="marker-stop" style="bottom:92%">
        <span class="marker-line"></span>
        <span class="marker-tag" id="tag-marker-stop">STOP 95%</span>
      </div>
      <!-- Start Line (Position clamped to always stay visible inside tank) -->
      <div class="tank-marker marker-start" id="marker-start" style="bottom:25%">
        <span class="marker-line"></span>
        <span class="marker-tag" id="tag-marker-start">START 25%</span>
      </div>
      <div class="tank-water" id="tank-water"></div>
    </div>

    <div class="tank-readout">
      <div class="tank-percent" id="tank-percent">--%</div>
      <div class="tank-status-label" id="tank-state">Checking Water...</div>
      <div id="tank-zone-badge" class="zone-badge zone-hold">Water OK</div>
    </div>
  </div>

  <!-- Mobile 3-Card Status (Water Level card removed as redundant) -->
  <div class="stats-grid">
    <div class="card">
      <div class="card-title">Pump Status</div>
      <div class="card-val" id="val-pump">STANDBY</div>
    </div>
    <div class="card">
      <div class="card-title">Motor Relay</div>
      <div class="card-val" id="val-relay">OFF</div>
    </div>
    <div class="card">
      <div class="card-title">Sensor Gap</div>
      <div class="card-val" id="val-distance">-- cm</div>
    </div>
  </div>
</div>

<!-- 1. PUMP CONTROLS -->
<div class="section-title">
  <span>&#127915; Pump Controls</span>
</div>
<div class="card" style="margin-bottom:12px;text-align:left">
  <div class="btn-group">
    <button class="btn btn-auto" id="btn-auto" onclick="setCmd('auto')">&#9654;&#9654; Auto</button>
    <button class="btn btn-on" id="btn-on" onclick="setCmd('on')">&#9654; Run</button>
    <button class="btn btn-off" id="btn-off" onclick="setCmd('off')">&#9632; Stop</button>
  </div>
  <div class="toast" id="msg-toast">&#8505;&#65039; Ready.</div>
</div>

<!-- 2. SENSOR SENSITIVITY -->
<div class="section-title">
  <span>&#9889; Sensor Interval</span>
  <span class="badge" id="badge-sensitivity-header" style="font-size:11px"><span class="dot dot-blue"></span><span id="txt-sensitivity-header">10s</span></span>
</div>
<div class="card" style="margin-bottom:12px;text-align:left">
  <div class="preset-grid">
    <button type="button" class="preset-btn" data-sec="2" onclick="setSensitivity(2)">2s</button>
    <button type="button" class="preset-btn" data-sec="5" onclick="setSensitivity(5)">5s</button>
    <button type="button" class="preset-btn active-preset" data-sec="10" onclick="setSensitivity(10)">10s</button>
    <button type="button" class="preset-btn" data-sec="15" onclick="setSensitivity(15)">15s</button>
    <button type="button" class="preset-btn" data-sec="30" onclick="setSensitivity(30)">30s</button>
    <button type="button" class="preset-btn" data-sec="60" onclick="setSensitivity(60)">60s</button>
  </div>

  <div class="custom-sensitivity-row">
    <label style="margin:0;font-size:12px;font-weight:700">Custom:</label>
    <input type="number" id="inp-custom-sec" class="custom-input" min="1" max="300" value="10">
    <span style="font-size:12px;color:var(--text-muted)">sec</span>
    <button type="button" class="custom-btn" onclick="applyCustomSensitivity()">Apply</button>
  </div>

  <div class="meter-wrapper">
    <div class="meter-label-row">
      <span id="lbl-meter-countdown">Next read in --s</span>
      <span id="lbl-meter-cycle">10s cycle</span>
    </div>
    <div class="meter-bar">
      <div class="meter-fill" id="meter-fill-bar"></div>
    </div>
  </div>
</div>

<!-- 3. CONFIGURATION -->
<div class="section-title">
  <span>&#9881;&#65039; Configuration</span>
</div>

<form class="card" id="cfg-form" onsubmit="saveCfg(event)" style="text-align:left">
  <div class="form-grid">
    <div class="form-item">
      <label>Tank Height (cm)</label>
      <input type="number" name="tankDepthCm" min="20" max="3000" value="120" required>
    </div>
    <div class="form-item">
      <label>Sensor Offset (cm)</label>
      <input type="number" name="sensorOffsetCm" value="0">
    </div>
    <div class="form-item">
      <label>Start Pump Below (%)</label>
      <input type="number" name="startPercent" min="0" max="99" value="25" required>
    </div>
    <div class="form-item">
      <label>Stop Pump At (%)</label>
      <input type="number" name="stopPercent" min="1" max="100" value="95" required>
    </div>
    <div class="form-item">
      <label>Max Run Time (min)</label>
      <input type="number" name="maxRunMinutes" min="1" max="240" value="30" required>
    </div>
    <div class="form-item">
      <label>Sensor Interval (sec)</label>
      <input type="number" name="sampleIntervalSec" min="1" max="300" value="10" required>
    </div>
    <div class="form-item">
      <label>Relay MAC (Optional)</label>
      <input type="text" name="relayMac" placeholder="AA:BB:CC:DD:EE:FF">
    </div>
    <div class="form-item">
      <label>Pairing Key</label>
      <input type="password" name="pairToken" placeholder="change-me" minlength="8" maxlength="16">
    </div>
    <div class="form-item">
      <label>New Password (Optional)</label>
      <input type="password" name="adminPassword" placeholder="Leave blank to keep">
    </div>
  </div>

  <div style="margin-top:12px;display:flex;align-items:center;gap:8px">
    <button type="submit" class="btn btn-auto" style="flex:1;padding:12px">&#128190; Save Configuration</button>
    <button type="button" class="copy-btn" onclick="copyMac()" title="Copy Controller MAC for ESP-01">Copy MAC</button>
  </div>
</form>
</div>

<script>
const $ = s => document.querySelector(s);
const $$ = s => document.querySelectorAll(s);
let currentMac = "";
let currentIntervalSec = 10;
let currentStartPercent = 25;
let currentStopPercent = 95;
let lastSampleAgeSec = 0;
let failCount = 0;
let isUpdating = false;

function copyMac() {
  if (!currentMac) return;
  navigator.clipboard.writeText(currentMac);
  $('#msg-toast').innerHTML = "&#9989; Copied controller MAC: " + currentMac;
}

function setMarkerPosition(el, pct) {
  // Clamp between 14px and calc(100% - 16px) so marker line & text tag are always 100% inside tank glass
  el.style.bottom = "clamp(14px, " + pct + "%, calc(100% - 16px))";
}

function updatePresetUI(sec) {
  currentIntervalSec = sec;
  $('#inp-custom-sec').value = sec;
  $('#txt-sensitivity-header').textContent = sec + "s";
  $('#lbl-meter-cycle').textContent = sec + "s cycle";
  $$('.preset-btn').forEach(b => {
    b.classList.toggle('active-preset', parseInt(b.getAttribute('data-sec')) === sec);
  });
}

async function setSensitivity(sec) {
  try {
    $('#msg-toast').innerHTML = "&#9203; Setting interval to " + sec + "s...";
    const res = await fetch('/api/sensitivity', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      credentials: 'same-origin',
      body: 'interval=' + sec
    });
    if (!res.ok) throw new Error(await res.text());
    const data = await res.json();
    updatePresetUI(data.sampleIntervalSec || sec);
    $('#msg-toast').innerHTML = "&#9989; Interval set to " + (data.sampleIntervalSec || sec) + "s";
    const inpForm = document.querySelector('input[name="sampleIntervalSec"]');
    if (inpForm) inpForm.value = sec;
    lastSampleAgeSec = 0;
    updateStatus();
  } catch(e) {
    $('#msg-toast').innerHTML = "&#10060; " + e.message;
  }
}

function applyCustomSensitivity() {
  const v = parseInt($('#inp-custom-sec').value);
  if (isNaN(v) || v < 1 || v > 300) {
    $('#msg-toast').innerHTML = "&#9888;&#65039; Interval: 1 to 300 sec";
    return;
  }
  setSensitivity(v);
}

function runMeterTick() {
  lastSampleAgeSec += 0.5;
  const cycle = Math.max(1, currentIntervalSec);
  const remaining = Math.max(0, cycle - lastSampleAgeSec);
  const pct = Math.min(100, Math.max(0, (lastSampleAgeSec / cycle) * 100));
  $('#meter-fill-bar').style.width = pct + "%";
  $('#lbl-meter-countdown').textContent = remaining > 0.5
    ? "Next read in ~" + Math.ceil(remaining) + "s"
    : "Reading sensor...";
}

async function updateStatus() {
  if (isUpdating) return;
  isUpdating = true;
  try {
    const res = await fetch('/api/status', { credentials: 'same-origin' });
    if (!res.ok) throw new Error("HTTP " + res.status);
    const d = await res.json();
    failCount = 0;
    currentMac = d.mac || "";
    $('#dev-info').textContent = "IP: http://" + (d.ip || "Connecting...") + " • http://iyum-water.local";

    if (d.sampleIntervalSec && d.sampleIntervalSec !== currentIntervalSec) {
      updatePresetUI(d.sampleIntervalSec);
    }
    if (d.sampleAge !== undefined) lastSampleAgeSec = d.sampleAge;

    if (d.startPercent !== undefined) {
      currentStartPercent = d.startPercent;
      setMarkerPosition($('#marker-start'), currentStartPercent);
      $('#tag-marker-start').textContent = "START " + currentStartPercent + "%";
    }
    if (d.stopPercent !== undefined) {
      currentStopPercent = d.stopPercent;
      setMarkerPosition($('#marker-stop'), currentStopPercent);
      $('#tag-marker-stop').textContent = "STOP " + currentStopPercent + "%";
    }

    const badgeZone = $('#tank-zone-badge');

    if (d.distance === null || isNaN(d.distance)) {
      $('#tank-percent').textContent = "--%";
      $('#tank-water').style.height = "0%";
      $('#val-distance').textContent = "--";
      $('#val-distance').style.color = "var(--warn)";
      $('#tank-state').textContent = "Sensor Offline";
      badgeZone.textContent = "Sensor Offline";
      badgeZone.className = "zone-badge zone-hold";
    } else {
      const pct = Math.max(0, Math.min(100, Math.round(d.level)));
      $('#tank-percent').textContent = pct + "%";
      $('#tank-water').style.height = pct + "%";
      $('#val-distance').textContent = d.distance.toFixed(1) + " cm";
      $('#val-distance').style.color = "#fff";

      if (pct <= currentStartPercent) {
        $('#tank-state').textContent = pct === 0 ? "Empty Tank" : "Low Water (" + pct + "%)";
        badgeZone.textContent = "⚡ Auto-Start (<= " + currentStartPercent + "%)";
        badgeZone.className = "zone-badge zone-fill";
      } else if (pct >= currentStopPercent) {
        $('#tank-state').textContent = "Tank Full (" + pct + "%)";
        badgeZone.textContent = "✓ Full (>= " + currentStopPercent + "%)";
        badgeZone.className = "zone-badge zone-full";
      } else {
        $('#tank-state').textContent = "Normal (" + pct + "%)";
        badgeZone.textContent = "Normal Range";
        badgeZone.className = "zone-badge zone-hold";
      }
    }

    $('#val-pump').textContent = d.pumpRequested ? "RUNNING" : "STANDBY";
    $('#val-pump').style.color = d.pumpRequested ? "var(--success)" : "var(--text-muted)";

    $('#val-relay').textContent = d.relayOn ? "ON" : "OFF";
    $('#val-relay').style.color = d.relayOn ? "var(--success)" : "var(--text-muted)";

    const dotR = $('#dot-relay');
    if (d.relayAge < 15) {
      dotR.className = "dot dot-green";
      $('#badge-relay').textContent = "Switch Linked";
    } else {
      dotR.className = "dot dot-amber";
      $('#badge-relay').textContent = "Switch Waiting";
    }

    $('#btn-auto').classList.toggle('active-mode', d.mode === 'auto');
    $('#btn-on').classList.toggle('active-mode', d.mode === 'on');
    $('#btn-off').classList.toggle('active-mode', d.mode === 'off');

    $('#badge-controller').textContent = "Online";
    const dotC = $('#badge-controller').previousElementSibling;
    if (dotC) dotC.className = "dot dot-green";

  } catch(err) {
    failCount++;
    if (failCount >= 3) {
      $('#badge-controller').textContent = "Connecting...";
      const dotC = $('#badge-controller').previousElementSibling;
      if (dotC) dotC.className = "dot dot-red";
    }
  } finally {
    isUpdating = false;
  }
}

async function setCmd(cmd) {
  try {
    $('#msg-toast').innerHTML = "&#9203; Sending...";
    const res = await fetch('/api/control', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      credentials: 'same-origin',
      body: 'command=' + cmd
    });
    const txt = await res.text();
    $('#msg-toast').innerHTML = "&#9989; " + txt;
    updateStatus();
  } catch(e) {
    $('#msg-toast').innerHTML = "&#10060; " + e.message;
  }
}

async function loadConfig() {
  try {
    const res = await fetch('/api/config', { credentials: 'same-origin' });
    if (!res.ok) return;
    const d = await res.json();
    for (let k in d) {
      const el = document.querySelector('[name="' + k + '"]');
      if (el && d[k] !== undefined && d[k] !== null && d[k] !== "") {
        el.value = d[k];
      }
    }
    if (d.sampleIntervalSec) updatePresetUI(d.sampleIntervalSec);
  } catch(e) {}
}

async function saveCfg(e) {
  e.preventDefault();
  try {
    $('#msg-toast').innerHTML = "&#9203; Saving...";
    const form = e.target;
    const res = await fetch('/api/config', {
      method: 'POST',
      credentials: 'same-origin',
      body: new URLSearchParams(new FormData(form))
    });
    const txt = await res.text();
    if (!res.ok) {
      $('#msg-toast').innerHTML = "&#10060; " + txt;
      return;
    }
    $('#msg-toast').innerHTML = "&#9989; " + txt;
    const sampleInp = form.querySelector('[name="sampleIntervalSec"]');
    if (sampleInp) updatePresetUI(parseInt(sampleInp.value) || 10);
    updateStatus();
  } catch(e) {
    $('#msg-toast').innerHTML = "&#10060; " + e.message;
  }
}

async function init() {
  await updateStatus();
  await loadConfig();
  setInterval(updateStatus, 3000);
  setInterval(runMeterTick, 500);
}

init();
</script>
</body>
</html>
)HTML";
