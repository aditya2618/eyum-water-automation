#pragma once
#include <Arduino.h>

const char DASHBOARD[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Eyum Water Automation</title>
<style>
:root{--bg:#090d16;--card:#131c2e;--card-border:rgba(255,255,255,0.08);--accent:#00c6ff;--accent2:#0072ff;--success:#10b981;--warn:#f59e0b;--danger:#ef4444;--text:#e2e8f0;--text-muted:#8094ae}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;background:linear-gradient(135deg,#070b12 0%,#0f172a 100%);color:var(--text);min-height:100vh;padding:20px 14px 40px}
.container{max-width:880px;margin:0 auto}
header{display:flex;flex-wrap:wrap;justify-content:space-between;align-items:center;gap:12px;margin-bottom:20px;padding-bottom:16px;border-bottom:1px solid var(--card-border)}
.logo-area{display:flex;align-items:center;gap:12px}
.logo-icon{width:42px;height:42px;border-radius:12px;background:linear-gradient(135deg,#00c6ff,#0072ff);display:flex;align-items:center;justify-content:center;box-shadow:0 0 20px rgba(0,198,255,0.4);font-size:22px}
h1{font-size:22px;font-weight:700;letter-spacing:-0.5px}
.subtitle{font-size:12px;color:var(--text-muted)}
.header-badges{display:flex;gap:8px;flex-wrap:wrap}
.badge{padding:6px 12px;border-radius:20px;font-size:12px;font-weight:600;display:inline-flex;align-items:center;gap:6px;background:rgba(255,255,255,0.05);border:1px solid var(--card-border)}
.dot{width:8px;height:8px;border-radius:50%;display:inline-block}
.dot-green{background:var(--success);box-shadow:0 0 10px var(--success)}
.dot-amber{background:var(--warn);box-shadow:0 0 10px var(--warn)}
.dot-red{background:var(--danger);box-shadow:0 0 10px var(--danger)}
.main-view{display:grid;grid-template-columns:220px 1fr;gap:16px;margin-bottom:16px}
@media(max-width:680px){.main-view{grid-template-columns:1fr}}
.tank-card{background:var(--card);border:1px solid var(--card-border);border-radius:18px;padding:20px 16px;display:flex;flex-direction:column;align-items:center;justify-content:center;box-shadow:0 8px 32px rgba(0,0,0,0.37)}
.tank-wrapper{width:130px;height:220px;border-radius:28px;border:3px solid rgba(255,255,255,0.18);position:relative;background:rgba(0,0,0,0.4);overflow:hidden;box-shadow:inset 0 0 20px rgba(0,0,0,0.6)}
.tank-scale{position:absolute;left:8px;top:0;bottom:0;width:100%;pointer-events:none;z-index:4;display:flex;flex-direction:column;justify-content:space-between;padding:12px 0;font-size:10px;color:rgba(255,255,255,0.4)}
.tank-water{position:absolute;bottom:0;left:0;right:0;height:0%;background:linear-gradient(180deg,#38bdf8 0%,#0284c7 50%,#0369a1 100%);transition:height 1s cubic-bezier(0.4,0,0.2,1);box-shadow:0 0 25px rgba(56,189,248,0.5)}
.tank-water::before{content:"";position:absolute;top:-8px;left:0;right:0;height:16px;background:rgba(255,255,255,0.3);border-radius:50%}
.tank-readout{margin-top:14px;text-align:center}
.tank-percent{font-size:32px;font-weight:800;letter-spacing:-1px;color:#fff}
.tank-status-label{font-size:12px;color:var(--text-muted);font-weight:600;text-transform:uppercase}
.stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px}
.card{background:var(--card);border:1px solid var(--card-border);border-radius:16px;padding:16px;box-shadow:0 4px 20px rgba(0,0,0,0.2);display:flex;flex-direction:column;justify-content:space-between}
.card-title{font-size:12px;color:var(--text-muted);font-weight:600;text-transform:uppercase;letter-spacing:0.5px;margin-bottom:8px}
.card-val{font-size:22px;font-weight:700;color:#fff}
.card-sub{font-size:11px;color:var(--text-muted);margin-top:6px}
.btn-group{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:10px;margin-top:14px}
.btn{position:relative;border:0;border-radius:12px;padding:14px 16px;font-size:14px;font-weight:700;cursor:pointer;color:#fff;background:#1e293b;border:1px solid var(--card-border);transition:all .2s cubic-bezier(0.4,0,0.2,1);display:flex;align-items:center;justify-content:center;gap:8px;box-shadow:0 4px 12px rgba(0,0,0,0.2)}
.btn:hover{transform:translateY(-2px);box-shadow:0 8px 24px rgba(0,0,0,0.4)}
.btn:active{transform:scale(0.95);box-shadow:0 2px 6px rgba(0,0,0,0.3)}
.btn-auto{background:linear-gradient(135deg,#2563eb,#1d4ed8);border-color:rgba(59,130,246,0.4)}
.btn-auto:hover{box-shadow:0 8px 25px rgba(37,99,235,0.5)}
.btn-on{background:linear-gradient(135deg,#059669,#047857);border-color:rgba(16,185,129,0.4)}
.btn-on:hover{box-shadow:0 8px 25px rgba(5,150,105,0.5)}
.btn-off{background:linear-gradient(135deg,#dc2626,#b91c1c);border-color:rgba(239,68,68,0.4)}
.btn-off:hover{box-shadow:0 8px 25px rgba(220,38,38,0.5)}
.btn.active-mode{outline:3px solid #38bdf8;outline-offset:2px}
.toast{padding:10px 16px;border-radius:10px;background:rgba(255,255,255,0.06);border:1px solid var(--card-border);font-size:13px;color:var(--text);margin-top:14px;display:flex;align-items:center;gap:8px}
.section-title{font-size:16px;font-weight:700;margin:24px 0 12px;display:flex;align-items:center;justify-content:space-between}
.config-card{background:var(--card);border:1px solid var(--card-border);border-radius:16px;padding:20px;box-shadow:0 4px 20px rgba(0,0,0,0.2)}
.form-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:14px}
label{display:block;font-size:12px;color:var(--text-muted);font-weight:600;margin-bottom:5px}
input{width:100%;padding:10px 12px;background:rgba(0,0,0,0.3);border:1px solid var(--card-border);border-radius:10px;color:#fff;font-size:14px;transition:border-color .2s}
input:focus{outline:0;border-color:var(--accent);box-shadow:0 0 12px rgba(0,198,255,0.3)}
.copy-btn{padding:4px 8px;font-size:11px;border-radius:6px;background:rgba(255,255,255,0.1);color:#fff;border:0;cursor:pointer;margin-left:6px}
.copy-btn:active{transform:scale(0.92)}
</style>
</head>
<body>
<div class="container">
<header>
  <div class="logo-area">
    <div class="logo-icon">&#128167;</div>
    <div>
      <h1>Eyum Water Automation</h1>
      <div class="subtitle" id="dev-info">ESP32 Tank Controller</div>
    </div>
  </div>
  <div class="header-badges">
    <div class="badge"><span class="dot dot-green"></span><span id="badge-controller">Online</span></div>
    <div class="badge"><span class="dot" id="dot-relay"></span><span id="badge-relay">Relay Link...</span></div>
  </div>
</header>

<div class="main-view">
  <div class="tank-card">
    <div class="tank-wrapper">
      <div class="tank-scale">
        <span>100%</span>
        <span>75%</span>
        <span>50%</span>
        <span>25%</span>
        <span>0%</span>
      </div>
      <div class="tank-water" id="tank-water"></div>
    </div>
    <div class="tank-readout">
      <div class="tank-percent" id="tank-percent">--%</div>
      <div class="tank-status-label" id="tank-state">Reading...</div>
    </div>
  </div>

  <div class="stats-grid">
    <div class="card">
      <div class="card-title">Sensor Distance</div>
      <div class="card-val" id="val-distance">-- cm</div>
      <div class="card-sub" id="sub-sensor">JSN-SR04M Trigger/Echo</div>
    </div>
    <div class="card">
      <div class="card-title">Pump Command</div>
      <div class="card-val" id="val-pump">STANDBY</div>
      <div class="card-sub" id="sub-mode">Automatic Mode</div>
    </div>
    <div class="card">
      <div class="card-title">Relay Output</div>
      <div class="card-val" id="val-relay">OFF</div>
      <div class="card-sub" id="sub-relay">Awaiting Link...</div>
    </div>
    <div class="card">
      <div class="card-title">Controller MAC</div>
      <div class="card-val" style="font-size:16px;word-break:break-all" id="val-mac">--</div>
      <div class="card-sub"><button class="copy-btn" onclick="copyMac()">Copy for ESP-01</button></div>
    </div>
  </div>
</div>

<div class="section-title">Pump Controls</div>
<div class="card">
  <div class="btn-group">
    <button class="btn btn-auto" id="btn-auto" onclick="setCmd('auto')">&#9654;&#9654; Automatic</button>
    <button class="btn btn-on" id="btn-on" onclick="setCmd('on')">&#9654; Run Pump</button>
    <button class="btn btn-off" id="btn-off" onclick="setCmd('off')">&#9632; Stop Pump</button>
  </div>
  <div class="toast" id="msg-toast">&#8505;&#65039; Initializing system...</div>
</div>

<div class="section-title">Configuration</div>
<form class="config-card" id="cfg-form" onsubmit="saveCfg(event)">
  <div class="form-grid">
    <div><label>Tank Depth (cm)</label><input type="number" name="tankDepthCm" min="20" required></div>
    <div><label>Sensor Offset (cm)</label><input type="number" name="sensorOffsetCm"></div>
    <div><label>Start Pump Below (%)</label><input type="number" name="startPercent" min="0" max="99" required></div>
    <div><label>Stop Pump At (%)</label><input type="number" name="stopPercent" min="1" max="100" required></div>
    <div><label>Max Run Time (min)</label><input type="number" name="maxRunMinutes" min="1" max="240" required></div>
    <div><label>Relay ESP-01 MAC</label><input type="text" name="relayMac" placeholder="AA:BB:CC:DD:EE:FF" required></div>
    <div><label>Pairing Token</label><input type="password" name="pairToken" maxlength="16" required></div>
    <div><label>New Dashboard Password</label><input type="password" name="adminPassword" placeholder="Leave blank to keep"></div>
  </div>
  <button type="submit" class="btn btn-auto" style="margin-top:16px;width:100%">&#128190; Save Configuration</button>
</form>
</div>

<script>
const $ = s => document.querySelector(s);
let currentMac = "";

function copyMac() {
  if (!currentMac) return;
  navigator.clipboard.writeText(currentMac);
  $('#msg-toast').innerHTML = "&#9989; Copied controller MAC (" + currentMac + ") to clipboard!";
}

async function updateStatus() {
  try {
    const res = await fetch('/api/status', { credentials: 'same-origin' });
    if (!res.ok) throw new Error("HTTP " + res.status);
    const d = await res.json();
    currentMac = d.mac || "";
    $('#val-mac').textContent = d.mac || "--";
    $('#dev-info').textContent = "IP: " + (d.ip || "Connecting...") + " • MAC: " + d.mac;

    if (d.distance === null || isNaN(d.distance)) {
      $('#tank-percent').textContent = "--%";
      $('#tank-water').style.height = "0%";
      $('#val-distance').textContent = "Disconnected";
      $('#val-distance').style.color = "var(--warn)";
      $('#sub-sensor').textContent = "⚠️ Check TRIG (GPIO18) / ECHO (GPIO19)";
      $('#tank-state').textContent = "Sensor Offline";
    } else {
      const pct = Math.max(0, Math.min(100, Math.round(d.level)));
      $('#tank-percent').textContent = pct + "%";
      $('#tank-water').style.height = pct + "%";
      $('#val-distance').textContent = d.distance.toFixed(1) + " cm";
      $('#val-distance').style.color = "#fff";
      $('#sub-sensor').textContent = "Sensor Active";
      $('#tank-state').textContent = pct > 90 ? "Full Tank" : (pct < 25 ? "Low Water" : "Normal Level");
    }

    $('#val-pump').textContent = d.pumpRequested ? "ACTIVE (ON)" : "STANDBY";
    $('#val-pump').style.color = d.pumpRequested ? "var(--success)" : "var(--text-muted)";
    $('#sub-mode').textContent = (d.mode.toUpperCase()) + " Mode";

    $('#val-relay').textContent = d.relayOn ? "MOTOR RUNNING" : "STOPPED";
    $('#val-relay').style.color = d.relayOn ? "var(--success)" : "var(--text-muted)";

    const dotR = $('#dot-relay');
    if (d.relayAge < 15) {
      dotR.className = "dot dot-green";
      $('#badge-relay').textContent = "ESP-01 Linked (" + d.relayAge + "s)";
      $('#sub-relay').textContent = "ESP-NOW Acknowledged";
    } else {
      dotR.className = "dot dot-amber";
      $('#badge-relay').textContent = "Relay Awaiting Ping";
      $('#sub-relay').textContent = "Check ESP-01 power";
    }

    $('#btn-auto').classList.toggle('active-mode', d.mode === 'auto');
    $('#btn-on').classList.toggle('active-mode', d.mode === 'on');
    $('#btn-off').classList.toggle('active-mode', d.mode === 'off');

  } catch(err) {
    $('#badge-controller').textContent = "Reconnecting...";
    $('#badge-controller').previousElementSibling.className = "dot dot-red";
  }
}

async function setCmd(cmd) {
  try {
    $('#msg-toast').innerHTML = "&#9203; Sending " + cmd.toUpperCase() + " command...";
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
    $('#msg-toast').innerHTML = "&#10060; Command failed: " + e.message;
  }
}

async function loadConfig() {
  try {
    const res = await fetch('/api/config', { credentials: 'same-origin' });
    if (!res.ok) return;
    const d = await res.json();
    for (let k in d) {
      const el = document.querySelector('[name="' + k + '"]');
      if (el) el.value = d[k];
    }
  } catch(e) {}
}

async function saveCfg(e) {
  e.preventDefault();
  try {
    $('#msg-toast').innerHTML = "&#9203; Saving configuration...";
    const form = e.target;
    const res = await fetch('/api/config', {
      method: 'POST',
      credentials: 'same-origin',
      body: new URLSearchParams(new FormData(form))
    });
    const txt = await res.text();
    $('#msg-toast').innerHTML = "&#9989; " + txt;
  } catch(e) {
    $('#msg-toast').innerHTML = "&#10060; Save failed: " + e.message;
  }
}

loadConfig();
updateStatus();
setInterval(updateStatus, 3000);
</script>
</body>
</html>
)HTML";
