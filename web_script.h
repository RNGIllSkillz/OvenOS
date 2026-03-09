#pragma once
#include <Arduino.h>

const char PAGE_JS[] PROGMEM = R"rawliteral(
var CUSTOM_PROF_IDX = 999;
var profileCache =[];

var mode = "manual";
var graphMode = "live"; 
var prevMsg = "";
var prevStep = -1;

var histTemps =[], histTemps2 =[], histSPs =[], histTS = [];
var liveTemps =[], liveTemps2 =[], liveSPs =[], liveTS =[];
var liveStartWall = 0;
var LIVE_MAX = 36000; 
var statusPending = false;
var historyPending = false;
var statusTimer = null;
var historyTimer = null;

var chartScale = 600;
var canvas = document.getElementById("chart");
var ctx = canvas.getContext("2d");
var isSettingsOpen = false; 

// --- LOCALIZATION LOGIC ---
function changeLang() {
  var sel = document.getElementById("langSel");
  localStorage.setItem("oven_lang", sel.value);
  location.reload();
}

function initI18n() {
  var sel = document.getElementById("langSel");
  if(sel) sel.value = localStorage.getItem('oven_lang') || 'en';

  document.querySelectorAll('[data-i18n]').forEach(function(el) {
    var key = el.getAttribute('data-i18n');
    if (LANG && LANG[key]) {
      if (el.tagName === 'INPUT' && el.type !== 'button') el.placeholder = LANG[key];
      else el.innerHTML = LANG[key];
    }
  });
  if (LANG) document.title = LANG.title;
}

// --- HELPER FUNCTIONS ---

function validateNumber(val, min, max, def) {
  var n = parseFloat(val);
  if (isNaN(n) || !isFinite(n)) return def;
  return Math.max(min, Math.min(max, n));
}

function fmtHMS(secs) {
  secs = validateNumber(secs, 0, 8640000, 0); 
  secs = Math.floor(secs);
  var h = Math.floor(secs / 3600);
  var m = Math.floor((secs % 3600) / 60);
  var s = secs % 60;
  var ms = ("0"+m).slice(-2), ss = ("0"+s).slice(-2);
  return h > 0 ? h+":"+ms+":"+ss : ms+":"+ss;
}

function escapeHtml(text) {
  var div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}

function sanitize(str) {
  return str.replace(/["'\\\/\x00-\x1f]/g, "").trim().slice(0, 31) || LANG.def_cust_name;
}

function addLog(msg, cls) {
  var log = document.getElementById("log");
  var t = new Date();
  var ts = ("0"+t.getHours()).slice(-2)+":"+("0"+t.getMinutes()).slice(-2)+":"+("0"+t.getSeconds()).slice(-2);
  var el = document.createElement("div");
  el.className = "le"+(cls?" "+cls:"");
  el.textContent = "["+ts+"] "+msg;
  log.insertBefore(el, log.firstChild);
  while (log.children.length > 50) log.removeChild(log.lastChild);
}

// --- CHARTING ---

function setScale(sec) {
  chartScale = validateNumber(sec, 0, 86400, 600);
  document.querySelectorAll(".tscale").forEach(function(b) {
    b.classList.toggle("on", +b.dataset.val === chartScale);
  });
  drawChart();
}

function setGraphMode(m) {
  graphMode = m;
  document.getElementById("btnGLive").classList.toggle("on", m === 'live');
  document.getElementById("btnGHist").classList.toggle("on", m === 'hist');
  drawChart();
}

function syncLiveGraph() {
  if (histTS.length === 0) {
    addLog(LANG.msg_sync_empty, "warn");
    return;
  }
  
  var cutoff = liveTS.length > 0 ? liveTS[0] : Infinity;
  var added = 0;
  var newTS =[], newTemps =[], newTemps2 =[], newSPs =[];
  
  for (var i = 0; i < histTS.length; i++) {
    if (histTS[i] < cutoff) {
      newTS.push(histTS[i]);
      newTemps.push(histTemps[i]);
      newTemps2.push(histTemps2[i]);
      newSPs.push(histSPs[i]);
      added++;
    }
  }
  
  if (added > 0) {
    liveTS = newTS.concat(liveTS);
    liveTemps = newTemps.concat(liveTemps);
    liveTemps2 = newTemps2.concat(liveTemps2);
    liveSPs = newSPs.concat(liveSPs);
    
    if (liveTS.length > LIVE_MAX) {
      var excess = liveTS.length - LIVE_MAX;
      liveTS = liveTS.slice(excess);
      liveTemps = liveTemps.slice(excess);
      liveTemps2 = liveTemps2.slice(excess);
      liveSPs = liveSPs.slice(excess);
    }
    
    addLog(LANG.msg_sync_added + added + LANG.msg_sync_pts, "success");
    if (graphMode === "live") drawChart();
  } else {
    addLog(LANG.msg_sync_skip, "ev");
  }
}

function drawChart() {
  var wrap = document.getElementById("canvasWrap");
  var W = wrap.clientWidth;
  var H = wrap.clientHeight; 
  if (W < 10) W = canvas.offsetWidth;
  if (H < 10) H = 220;

  if (canvas.width !== W || canvas.height !== H) {
    canvas.width = W; 
    canvas.height = H;
  }
  
  var pad = {t:16, r:20, b:36, l:46};
  var cw = W - pad.l - pad.r, ch = H - pad.t - pad.b;

  ctx.fillStyle = "#0d0f10";
  ctx.fillRect(0, 0, W, H);

  var allTemps  = graphMode === "live" ? liveTemps : histTemps;
  var allTemps2 = graphMode === "live" ? liveTemps2 : histTemps2;
  var allSPs    = graphMode === "live" ? liveSPs   : histSPs;
  var allTS     = graphMode === "live" ? liveTS    : histTS;

  if (allTS.length === 0) {
    ctx.fillStyle = "#4a5058"; 
    ctx.font = "13px 'Share Tech Mono'";
    ctx.textAlign = "center";
    ctx.fillText(graphMode === "live" ? LANG.chart_wait_live : LANG.chart_wait_hist, W/2, H/2);
    return;
  }
  
  var visTemps, visTemps2, visSPs, visTS;
  
  if (chartScale === 0) {
    visTemps = allTemps; visTemps2 = allTemps2; visSPs = allSPs; visTS = allTS;
  } else {
    var tCutoff = allTS[allTS.length-1] - chartScale;
    var startIdx = 0;
    for (var si = 0; si < allTS.length; si++) {
      if (allTS[si] >= tCutoff) { startIdx = si; break; }
    }
    visTemps = allTemps.slice(startIdx);
    visTemps2 = allTemps2.slice(startIdx);
    visSPs = allSPs.slice(startIdx);
    visTS = allTS.slice(startIdx);
  }

  if (visTS.length < 2) return;

  var yMin = Infinity, yMax = -Infinity;
  for (var m = 0; m < visTemps.length; m++) {
      if (visTemps[m] < yMin) yMin = visTemps[m];
      if (visTemps[m] > yMax) yMax = visTemps[m];
  }
  for (var m = 0; m < visSPs.length; m++) {
      if (visSPs[m] < yMin) yMin = visSPs[m];
      if (visSPs[m] > yMax) yMax = visSPs[m];
  }
  for (var m = 0; m < visTemps2.length; m++) {
      if (visTemps2[m] > 0.1) { // Ignore 0 (Disconnected TC2)
          if (visTemps2[m] < yMin) yMin = visTemps2[m];
          if (visTemps2[m] > yMax) yMax = visTemps2[m];
      }
  }

  if (yMin === Infinity) yMin = 0;
  if (yMax === -Infinity) yMax = 100;

  var yPad = (yMax - yMin) * 0.15 + 5;
  yMin = Math.max(0, yMin - yPad); 
  yMax = yMax + yPad;
  var yRng = yMax - yMin || 1;
  var xMax = visTS[visTS.length-1];
  var xMin = chartScale > 0 ? Math.max(0, xMax - chartScale) : visTS[0];
  var xRng = xMax - xMin || 1;

  function px(x) { return pad.l + (x - xMin) / xRng * cw; }
  function py(y) { return pad.t + ch - (y - yMin) / yRng * ch; }

  // Draw Grids
  ctx.strokeStyle = "#1e2427"; 
  ctx.lineWidth = 1;
  for (var i = 0; i <= 5; i++) {
    var yv = yMin + yRng/5*i, yp = py(yv);
    ctx.beginPath(); ctx.moveTo(pad.l, yp); ctx.lineTo(pad.l+cw, yp); ctx.stroke();
    ctx.fillStyle = "#4a5058"; ctx.font = "10px 'Share Tech Mono'"; ctx.textAlign = "right"; 
    ctx.fillText(Math.round(yv)+"", pad.l-5, yp+3);
  }

  var xSteps = 5;
  for (var j = 0; j <= xSteps; j++) {
    var xv = xMin + xRng/xSteps*j, xp2 = px(xv);
    ctx.strokeStyle = "#1e2427"; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(xp2, pad.t); ctx.lineTo(xp2, pad.t+ch); ctx.stroke();
    ctx.fillStyle = "#4a5058"; ctx.font = "10px 'Share Tech Mono'"; ctx.textAlign = "center";
    ctx.fillText(fmtHMS(Math.round(xv)), xp2, pad.t+ch+18);
  }

  ctx.save();
  ctx.beginPath(); ctx.rect(pad.l, pad.t, cw, ch); ctx.clip();

  // Setpoint Line (Blue Dashed)
  ctx.strokeStyle = "#3ab0e8"; ctx.lineWidth = 1.5; ctx.setLineDash([6,4]);
  ctx.beginPath();
  for (var k = 0; k < visSPs.length; k++) {
    if (k===0) ctx.moveTo(px(visTS[k]), py(visSPs[k]));
    else ctx.lineTo(px(visTS[k]), py(visSPs[k]));
  }
  ctx.stroke(); 
  ctx.setLineDash([]);

  // TC1 Air Area Fill
  var grad = ctx.createLinearGradient(0, pad.t, 0, pad.t+ch);
  grad.addColorStop(0, "rgba(232,144,10,0.22)");
  grad.addColorStop(1, "rgba(232,144,10,0)");
  ctx.fillStyle = grad;
  ctx.beginPath();
  ctx.moveTo(px(visTS[0]), pad.t+ch);
  for (var n = 0; n < visTemps.length; n++) ctx.lineTo(px(visTS[n]), py(visTemps[n]));
  ctx.lineTo(px(visTS[visTemps.length-1]), pad.t+ch);
  ctx.closePath(); 
  ctx.fill();

  // TC1 Air Line (Orange)
  ctx.strokeStyle = "#e8900a"; ctx.lineWidth = 2;
  ctx.beginPath();
  for (var p2 = 0; p2 < visTemps.length; p2++) {
    if (p2===0) ctx.moveTo(px(visTS[p2]), py(visTemps[p2]));
    else ctx.lineTo(px(visTS[p2]), py(visTemps[p2]));
  }
  ctx.stroke();
  
  // TC2 Part Line (Green)
  ctx.strokeStyle = "#2ecc71"; ctx.lineWidth = 2; 
  ctx.beginPath();
  var t2Active = false;
  for (var p3 = 0; p3 < visTemps2.length; p3++) {
    if (visTemps2[p3] > 0.1) {
      if (!t2Active) { ctx.moveTo(px(visTS[p3]), py(visTemps2[p3])); t2Active = true; }
      else { ctx.lineTo(px(visTS[p3]), py(visTemps2[p3])); }
    } else {
      t2Active = false; // Break line if sensor unplugs
    }
  }
  ctx.stroke();
  ctx.restore();

  // Legends
  ctx.fillStyle="#e8900a"; ctx.fillRect(pad.l,H-14,16,3);
  ctx.fillStyle="#d4cfc9"; ctx.font="10px 'Share Tech Mono'"; ctx.textAlign="left";
  ctx.fillText(LANG.chart_lbl_temp || "Air", pad.l+20, H-10);

  ctx.strokeStyle="#3ab0e8"; ctx.lineWidth=1.5; ctx.setLineDash([4,3]);
  ctx.beginPath(); ctx.moveTo(pad.l+50,H-12); ctx.lineTo(pad.l+66,H-12); ctx.stroke();
  ctx.setLineDash([]); ctx.fillStyle="#d4cfc9";
  ctx.fillText(LANG.chart_lbl_target || "Target", pad.l+70, H-10);

  ctx.fillStyle="#2ecc71"; ctx.fillRect(pad.l+120,H-14,16,3);
  ctx.fillStyle="#d4cfc9"; 
  var tc2Text = LANG.tc2_label ? LANG.tc2_label.replace(':','') : "Part";
  ctx.fillText(tc2Text, pad.l+140, H-10);
}

// --- PROFILE LOADING ---

function loadProfiles() {
  var sel = document.getElementById("profSel");
  sel.innerHTML = "<option>" + LANG.prof_loading + "</option>";
  sel.disabled = true;

  fetch("/profiles")
    .then(r => r.json())
    .then(data => {
      profileCache = data; 
      sel.innerHTML = "";
      
      data.forEach((p, index) => {
        var opt = document.createElement("option");
        opt.value = index;
        var tName = (LANG.backend && LANG.backend[p.name]) ? LANG.backend[p.name] : p.name;
        opt.textContent = tName;
        sel.appendChild(opt);
      });
      
      sel.disabled = false;
      CUSTOM_PROF_IDX = data.length; 
      renderPreview(); 
    })
    .catch(e => {
      sel.innerHTML = "<option>" + LANG.prof_err + "</option>";
      addLog(LANG.prof_err + ": " + e, "err");
    });
}

function renderPreview() {
  var idx = parseInt(document.getElementById("profSel").value);
  if (isNaN(idx) || idx < 0 || !profileCache[idx]) return;
  
  var steps = profileCache[idx].steps;
  var h = "";
  for (var i=0; i<steps.length; i++) {
    var rawLabel = steps[i].l;
    var tLabel = (LANG.backend && LANG.backend[rawLabel]) ? LANG.backend[rawLabel] : rawLabel;
    h += "<div class='prow'><span class='pname'>"+escapeHtml(tLabel)+"</span><span>"+steps[i].t+"&deg;C / "+steps[i].h+" "+LANG.lbl_min+"</span></div>";
  }
  document.getElementById("profPreview").innerHTML = h;
}

// --- UI INTERACTION ---

function toggleSettings() {
  isSettingsOpen = !isSettingsOpen;
  var main = document.getElementById("mainControls");
  var sets = document.getElementById("settingsPane");
  var title = document.getElementById("ctrlTitle");
  
  if (isSettingsOpen) {
    main.classList.add("hidden");
    sets.classList.add("on");
    title.textContent = LANG.ctrl_settings;
  } else {
    main.classList.remove("hidden");
    sets.classList.remove("on");
    title.textContent = LANG.ctrl_title;
  }
}

function switchMode(m) {
  mode = m;
  var tabs = document.querySelectorAll(".tab");
  tabs[0].classList.toggle("on", m==="manual");
  tabs[1].classList.toggle("on", m==="profile");
  tabs[2].classList.toggle("on", m==="custom");
  document.getElementById("manGrp").classList.toggle("on", m==="manual");
  document.getElementById("profGrp").classList.toggle("on", m==="profile");
  document.getElementById("customGrp").classList.toggle("on", m==="custom");
  if (m==="profile") renderPreview();
  if (m==="custom" && document.getElementById("stepList").children.length===0) {
    addCustomStep(LANG.def_step_heat, 150, 30);
    addCustomStep(LANG.def_step_cool, 25, 10);
  }
}

var stepCounter = 0;
function addCustomStep(label, temp, hold) {
  label = label || LANG.step_def; 
  temp = validateNumber(temp, 0, 280, 100); 
  hold = validateNumber(hold, 1, 600, 10);
  
  var id = "step_" + (stepCounter++);
  var div = document.createElement("div");
  div.className = "step-row"; 
  div.id = id;
  
  var inLabel = document.createElement("input"); 
  inLabel.type="text"; inLabel.value=label; inLabel.maxLength=23; inLabel.placeholder=LANG.step_placeholder;
  var inTemp = document.createElement("input"); 
  inTemp.type="number"; inTemp.value=temp; inTemp.min=0; inTemp.max=280; 
  var inHold = document.createElement("input"); 
  inHold.type="number"; inHold.value=hold; inHold.min=1; inHold.max=600; 
  var btnDel = document.createElement("button"); 
  btnDel.className="step-del"; btnDel.textContent="×";
  btnDel.onclick = function(){ var el = document.getElementById(id); if(el) el.remove(); };
  
  div.appendChild(inLabel); div.appendChild(inTemp); div.appendChild(inHold); div.appendChild(btnDel);
  document.getElementById("stepList").appendChild(div);
}

function getCustomSteps() {
  var rows = document.getElementById("stepList").querySelectorAll(".step-row");
  var steps =[];
  for (var i=0; i<rows.length; i++) {
    var inputs = rows[i].querySelectorAll("input");
    if (inputs.length < 3) continue;
    var label = sanitize(inputs[0].value) || LANG.step_def;
    var temp = validateNumber(inputs[1].value, 0, 280, 25);
    var hold = validateNumber(inputs[2].value, 1, 600, 1);
    inputs[0].value = label; inputs[1].value = temp; inputs[2].value = hold;
    steps.push({ label:label, temp:temp, hold:hold });
  }
  return steps;
}

// --- COMMANDS ---

function setRunning(r) {
  if (!isSettingsOpen) {
      document.getElementById("btnStart").disabled = !!r;
  }
}

function doStart() {
  var fd = new FormData();
  
  setRunning(true);
  histTemps =[]; histSPs =[]; histTS =[];
  liveTemps =[]; liveSPs =[]; liveTS =[];
  liveStartWall = 0;
  drawChart();

  if (mode === "manual") {
    var temp = validateNumber(document.getElementById("tTemp").value, 0, 280, 100);
    var time = validateNumber(document.getElementById("tDur").value, 1, 600, 10);
    fd.append("mode","manual"); fd.append("temp", temp); fd.append("time", time);
    addLog(LANG.msg_start_man + temp + "°C / " + time + " " + LANG.lbl_min, "ev");
    
    fetch("/start",{method:"POST", headers:{"X-Oven-Auth":"1"}, body:fd})
      .then(r => { if(r.status === 409) return r.text().then(t => { throw new Error(t); }); if(!r.ok) throw new Error("HTTP "+r.status); })
      .catch(e => { setRunning(false); addLog(LANG.msg_start_err + e, "err"); });

  } else if (mode === "profile") {
    var pi = parseInt(document.getElementById("profSel").value);
    if (isNaN(pi) || pi < 0 || pi >= profileCache.length) {
         addLog(LANG.msg_prof_invalid, "err"); setRunning(false); return;
    }
    fd.append("mode","profile"); fd.append("profile", pi);
    var tName = (LANG.backend && LANG.backend[profileCache[pi].name]) ? LANG.backend[profileCache[pi].name] : profileCache[pi].name;
    addLog(LANG.msg_start_prof + tName, "ev");
    fetch("/start",{method:"POST", headers:{"X-Oven-Auth":"1"}, body:fd})
      .then(r => { if(!r.ok) return r.text().then(t => { throw new Error(t); }); })
      .catch(e => { setRunning(false); addLog(LANG.msg_start_err + e, "err"); });

  } else {
    var steps = getCustomSteps();
    if (steps.length === 0) { alert(LANG.msg_prof_req_step); setRunning(false); return; }
    var name = sanitize(document.getElementById("custName").value) || LANG.def_cust_name;
    document.getElementById("custName").value = name;
    var profJson = JSON.stringify({name:name, steps:steps});
    addLog(LANG.msg_start_prof + "'" + name + "'", "ev");
    
    fetch("/setcustom", {method:"POST",headers:{"Content-Type":"application/json", "X-Oven-Auth":"1"},body:profJson})
    .then(r => {
      if(!r.ok) return r.text().then(t => { throw new Error(t); });
      var fd2 = new FormData();
      fd2.append("mode","profile"); fd2.append("profile", String(CUSTOM_PROF_IDX));
      return fetch("/start",{method:"POST", headers:{"X-Oven-Auth":"1"}, body:fd2});
    })
    .then(r => { if(!r.ok) return r.text().then(t => { throw new Error(t); }); })
    .catch(e => { setRunning(false); addLog(LANG.msg_start_err + e.message, "err"); });
  }
}

function saveCustomProfile() {
  var steps = getCustomSteps();
  if (steps.length === 0) { alert(LANG.msg_prof_req_step); return; }
  var btn = document.querySelector(".bsave");
  btn.innerHTML = LANG.btn_saving; btn.disabled = true;
  var name = sanitize(document.getElementById("custName").value) || LANG.def_cust_name;
  document.getElementById("custName").value = name;
  var profJson = JSON.stringify({name:name, steps:steps});
  
  fetch("/setcustom", {method:"POST",headers:{"Content-Type":"application/json", "X-Oven-Auth":"1"},body:profJson})
  .then(r => {
    if (!r.ok) return r.text().then(t => { throw new Error(t); });
    btn.innerHTML = LANG.btn_saved; btn.classList.add("saved"); btn.disabled = false;
    addLog(LANG.msg_prof_saved + "'" + name + "'", "success");
    setTimeout(() => { btn.innerHTML = LANG.btn_save; btn.classList.remove("saved"); }, 2500);
  })
  .catch((e) => {
    btn.innerHTML = LANG.btn_save_err; btn.disabled = false;
    addLog(LANG.msg_prof_save_err + ": " + e.message, "err");
    setTimeout(() => { btn.innerHTML = LANG.btn_save; }, 2500);
  });
}

function savePID() {
  var fd = new FormData();
  fd.append("kp", document.getElementById("pidKp").value);
  fd.append("ki", document.getElementById("pidKi").value);
  fd.append("kd", document.getElementById("pidKd").value);
  
  fetch("/setpid", {method:"POST", headers:{"X-Oven-Auth":"1"}, body:fd})
    .then(r => {
       if(r.ok) { addLog(LANG.msg_pid_ok, "success"); toggleSettings(); }
       else addLog(LANG.msg_pid_err, "err");
    });
}

function doStop() {
  fetch("/stop",{method:"POST", headers:{"X-Oven-Auth":"1"}})
    .then(r => {
        if (!r.ok) throw new Error("HTTP "+r.status);
        setRunning(false);
        addLog(LANG.msg_stop_ok, "warn");
    })
    .catch(e => {
        addLog(LANG.msg_stop_err + e, "err");
    });
}

function doReset() {
  fetch("/reset",{method:"POST", headers:{"X-Oven-Auth":"1"}})
  .then(r => { if(r.ok) addLog(LANG.msg_reset_ok, "success"); else addLog(LANG.msg_reset_err, "err"); });
}

// --- POLLING ---

var failCount = 0;
function scheduleStatus(delay) {
  clearTimeout(statusTimer);
  statusTimer = setTimeout(pollStatus, Math.max(2000, delay)); 
}

function toggleFan() {
  var btn = document.getElementById("btnFan");
  var isFanOn = btn.classList.contains("fan-active");
  var newState = isFanOn ? "0" : "1";

  var fd = new FormData();
  fd.append("state", newState);

  fetch("/fan", {method:"POST", headers:{"X-Oven-Auth":"1"}, body:fd})
    .then(r => {
        if(r.ok) {
            if(newState === "1") {
                btn.classList.add("fan-active");
                btn.style.borderColor = "var(--blu)";
                btn.style.color = "var(--blu)";
            } else {
                btn.classList.remove("fan-active");
                btn.style.borderColor = "var(--bdr)";
                btn.style.color = "var(--mut)";
            }
        } else {
            console.error("Fan toggle failed");
        }
    });
}

function toggleMonitor() {
  var btn = document.getElementById("btnMon");
  var isMon = btn.classList.contains("mon-active");
  var newState = isMon ? "0" : "1";

  var fd = new FormData();
  fd.append("state", newState);

  fetch("/monitor", {method:"POST", headers:{"X-Oven-Auth":"1"}, body:fd})
    .then(r => {
        if(r.ok) {
            if(newState==="1") {
                addLog(LANG.msg_mon_start, "ev");
                histTemps=[]; histSPs=[]; histTS=[]; liveTemps=[]; liveSPs=[]; liveTS=[];
                liveStartWall = 0; 
            } else {
                addLog(LANG.msg_mon_stop, "ev");
            }
        }
    });
}

function pollStatus() {
  if (statusPending) { scheduleStatus(2500); return; }
  statusPending = true;
  var t0 = Date.now();
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), 5000); 

  fetch("/status", { signal: controller.signal })
    .then(r => { clearTimeout(timeoutId); return r.json(); })
    .then(d => {
      failCount = 0; statusPending = false;
      var badge = document.getElementById("badge");
      var btnStart = document.getElementById("btnStart");
      var btnStop = document.getElementById("btnStop");
      var btnReset = document.getElementById("btnReset");
      var btnGear = document.getElementById("btnGear");
      var btnMon = document.getElementById("btnMon");

      var wb = document.getElementById("wifi-badge");
      var rssi = d.rssi !== undefined ? parseInt(d.rssi) : -100;

      if (rssi === 0) {
        wb.textContent = "WiFi: AP Mode";
        wb.className = "wifi-badge wifi-ok";
      } else {
        wb.textContent = "WiFi: " + rssi + "dBm";
        wb.className = "wifi-badge";
        if (rssi > -60) wb.classList.add("wifi-good");
        else if (rssi > -75) wb.classList.add("wifi-ok");
        else wb.classList.add("wifi-bad");
      }

      var cpuB = document.getElementById("cpu-badge");
      if (d.cpuTemp !== undefined) {
          var ct = parseFloat(d.cpuTemp);
          cpuB.innerHTML = "CPU: " + ct.toFixed(1) + "&deg;C";
          
          // Color code based on ESP32 operating temps
          if (ct > 75) { 
              cpuB.style.color = "var(--red)"; 
              cpuB.style.borderColor = "var(--red)"; 
          } else if (ct > 60) { 
              cpuB.style.color = "var(--amb)"; 
              cpuB.style.borderColor = "var(--amb)"; 
          } else { 
              cpuB.style.color = "var(--mut)"; 
              cpuB.style.borderColor = "var(--bdr)"; 
          }
      }

      wb.className = "wifi-badge";
      if (rssi > -60) wb.classList.add("wifi-good");
      else if (rssi > -75) wb.classList.add("wifi-ok");
      else wb.classList.add("wifi-bad");

      if (d.running) { btnGear.disabled = true; if(isSettingsOpen) toggleSettings(); } 
      else { btnGear.disabled = false; }

      if (d.emergency) {
          badge.textContent = LANG.badge_alarm; badge.className = "crit";
          btnStart.style.display = "none"; btnStop.style.display = "none"; btnReset.style.display = "block";
      } else {
          badge.textContent = "* " + LANG.badge_online; badge.className = "live";
          btnStart.style.display = "block"; btnStop.style.display = "block"; btnReset.style.display = "none";
      }

      if (d.monitoring) {
          btnMon.classList.add("mon-active");
          btnMon.style.borderColor = "var(--blu)";
          btnMon.style.color = "var(--blu)";
          btnMon.innerHTML = LANG.btn_monitor_stop;
      } else {
          btnMon.classList.remove("mon-active");
          btnMon.style.borderColor = "var(--bdr)";
          btnMon.style.color = "var(--mut)";
          btnMon.innerHTML = LANG.btn_monitor;
      }
      btnMon.disabled = d.running;

      // Handle FAN State visually
      var btnFan = document.getElementById("btnFan");
      if (d.fan) {
          btnFan.classList.add("fan-active");
          btnFan.style.borderColor = "var(--blu)";
          btnFan.style.color = "var(--blu)";
      } else {
          btnFan.classList.remove("fan-active");
          btnFan.style.borderColor = "var(--bdr)";
          btnFan.style.color = "var(--mut)";
      }

      // Handle TC2 Board Temp Visibility
      var tc2Div = document.getElementById("tc2Div");
      if (d.hasTC2) {
          tc2Div.style.display = "block";
          var temp2 = validateNumber(d.temp2, -50, 500, 0);
          document.getElementById("curT2").textContent = temp2.toFixed(1);
      } else {
          tc2Div.style.display = "none";
      }

      var temp = validateNumber(d.temp, -50, 500, 0);
      var temp2 = validateNumber(d.temp2, -50, 500, 0);
      var sp = d.setpoint ? validateNumber(d.setpoint, 0, 300, 0) : 0;
      var el = document.getElementById("curT");
      el.textContent = temp.toFixed(1);
      el.className = "tbig "+(temp>150?"hot":temp<40?"cool":"");
     

      var elapsed = validateNumber(d.elapsed, 0, 86400000, 0);
      if (liveStartWall === 0) {
        liveStartWall = Date.now() - (elapsed * 1000);
      } else {
        var expectedLive = (Date.now() - liveStartWall) / 1000;
        if (Math.abs(expectedLive - elapsed) > 5) {
          liveStartWall = Date.now() - (elapsed * 1000);
          if (elapsed < expectedLive && liveTS.length > 0 && elapsed < liveTS[liveTS.length-1]) {
             liveTS =[]; liveTemps =[]; liveTemps2 =[]; liveSPs =[];
          }
        }
      }

      var liveNow = (Date.now() - liveStartWall) / 1000;
      if (liveTS.length === 0 || liveNow > liveTS[liveTS.length-1]) {
         liveTemps.push(temp);
         liveTemps2.push(temp2);
         liveSPs.push(sp); 
         liveTS.push(liveNow);
         if (liveTS.length > LIVE_MAX) { liveTemps.shift(); liveSPs.shift(); liveTS.shift(); }
         if (graphMode === "live") drawChart();
      }

      document.getElementById("spV").textContent = sp ? sp.toFixed(0) : LANG.target_idle;

      // Translate the Backend C++ Response Key into User's Language
      var rawMsg = String(d.msg || "UNKNOWN");
      var translatedMsg = (LANG.backend && LANG.backend[rawMsg]) ? LANG.backend[rawMsg] : rawMsg;

      // Handle custom trailing Alarm messages
      if (rawMsg.startsWith("ALARM: ")) {
          var reason = rawMsg.substring(7);
          var translatedReason = (LANG.backend && LANG.backend[reason]) ? LANG.backend[reason] : reason;
          var alarmPrefix = (LANG.backend && LANG.backend["ALARM: "]) ? LANG.backend["ALARM: "] : "ALARM: ";
          translatedMsg = alarmPrefix + translatedReason;
      }
      
      var pill = document.getElementById("pill");
      pill.textContent = translatedMsg; 
      pill.className = "pill";
      
      // Dynamic translation-safe color logic evaluates ENGLISH RAW strings only
      if (rawMsg.startsWith("ALARM:") || rawMsg === "STOPPED") {
          pill.classList.add("err");
      } else if (rawMsg === "HOLD" || rawMsg === "DONE" || rawMsg === "END") {
          pill.classList.add("hold");
      } else if (rawMsg !== "WAITING" && rawMsg !== "IDLE") {
          pill.classList.add("heat"); 
      }

      if(!d.emergency) setRunning(d.running);

      var timeLeft = validateNumber(d.timeLeft, 0, 86400, 0);
      var holdMinV = validateNumber(d.holdMin, 0, 600, 0);
      if (timeLeft > 0 && holdMinV > 0) {
        document.getElementById("timer").textContent = fmtHMS(timeLeft);
        var pct = Math.min(Math.max((1-timeLeft/(holdMinV*60))*100,0),100);
        document.getElementById("pbar").style.width = pct+"%";
      } else {
        document.getElementById("timer").textContent = "--:--:--";
        if (!d.running) document.getElementById("pbar").style.width = "0%";
      }

      var sn = document.getElementById("stepName");
      var sb = document.getElementById("stepSub");
      var ds = document.getElementById("dots");
      if (d.profStep >= 0 && d.profName) {
        var tProfName = (LANG.backend && LANG.backend[d.profName]) ? LANG.backend[d.profName] : d.profName;
        sn.textContent = tProfName;
        sb.textContent = LANG.msg_step + (d.profStep+1) + LANG.msg_of + d.profSteps + " — " + translatedMsg;
        var dh = "";
        for (var i=0; i<d.profSteps; i++) {
          var dc = i<d.profStep?"done":(i===d.profStep?"active":"");
          dh += "<div class='dot "+dc+"'></div>";
        }
        ds.innerHTML = dh;
        if (d.profStep !== prevStep) { addLog(LANG.msg_step + (d.profStep+1) + "/" + d.profSteps + ": " + translatedMsg, "ev"); prevStep = d.profStep; }
      } else {
        sn.textContent = d.running ? LANG.msg_manual_mode : "--"; sb.textContent=""; ds.innerHTML="";
      }

      if (rawMsg !== prevMsg) { 
        var logCls = "ev";
        if (rawMsg.startsWith("ALARM:") || rawMsg === "STOPPED") logCls = "err";
        else if (rawMsg === "HOLD" || rawMsg === "DONE" || rawMsg === "END") logCls = "success";
        
        addLog(LANG.msg_state + translatedMsg, logCls); prevMsg = rawMsg; 
      }
      
      if (d.kp !== undefined && !isSettingsOpen) {
         document.getElementById("pidKp").value = d.kp;
         document.getElementById("pidKi").value = d.ki;
         document.getElementById("pidKd").value = d.kd;
      }
      
      var elapsedDraw = Date.now() - t0;
      scheduleStatus(elapsedDraw > 800 ? 3000 : 2000);
    })
    .catch((e) => {
      console.error("Status Poll Error:", e);
      statusPending = false; failCount++;
      if (failCount > 3) { document.getElementById("badge").textContent = LANG.msg_dead; document.getElementById("badge").className = ""; }
      scheduleStatus(3000); 
    });
}

function scheduleHistory(delay) {
  clearTimeout(historyTimer);
  historyTimer = setTimeout(pollHistory, delay);
}

function pollHistory() {
  if (historyPending) { scheduleHistory(30000); return; }
  historyPending = true;

  fetch("/history")
    .then(r => r.arrayBuffer())
    .then(buf => {
      historyPending = false;
      
      if (buf.byteLength < 2) {
        histTemps =[]; histTemps2 =[]; histSPs = []; histTS =[];
        if (graphMode === "hist") drawChart();
        scheduleHistory(30000);
        return;
      }

      const dv = new DataView(buf);
      let offset = 0;
      const count = dv.getUint16(offset, true); offset += 2;

      if (buf.byteLength < 2 + (count * 8)) {
        scheduleHistory(30000);
        return;
      }

      histTemps = new Array(count);
      histTemps2 = new Array(count);
      histSPs   = new Array(count);
      histTS    = new Array(count);

      // Unpack 8-byte blocks (Time, Temp, Temp2, Setpoint)
      for (let i = 0; i < count; i++) {
        histTS[i]     = dv.getUint16(offset, true); offset += 2;
        histTemps[i]  = dv.getInt16(offset, true) / 10.0; offset += 2;
        histTemps2[i] = dv.getInt16(offset, true) / 10.0; offset += 2; 
        histSPs[i]    = dv.getInt16(offset, true); offset += 2;
      }

      if (graphMode === "hist") drawChart();
      scheduleHistory(30000);
    })
    .catch((e) => { 
      console.error("History Poll Error:", e);
      historyPending = false; 
      scheduleHistory(30000); 
    });
}

// --- Graph Export ---

function exportChart() {
  var visTemps  = graphMode === "live" ? liveTemps : histTemps;
  var visTemps2 = graphMode === "live" ? liveTemps2 : histTemps2;
  var visSPs    = graphMode === "live" ? liveSPs   : histSPs;
  var visTS     = graphMode === "live" ? liveTS    : histTS;

  if (visTS.length < 2) {
    alert(LANG.msg_export_err || "Not enough data to export.");
    return;
  }

  // Asynchronously load the Favicon before rendering the chart
  var img = new Image();
  img.onload = function() { renderAndDownload(img); };
  img.onerror = function() { renderAndDownload(null); }; // Fallback if image fails
  img.src = "/favicon.ico";

  function renderAndDownload(iconImg) {
    var W = 1200;
    var H = 600;
    var offCanvas = document.createElement("canvas");
    offCanvas.width = W;
    offCanvas.height = H;
    var ctxOff = offCanvas.getContext("2d");

    ctxOff.fillStyle = "#ffffff";
    ctxOff.fillRect(0, 0, W, H);

    // Keep the original padding so the graph is full size
    var pad = { t: 60, r: 40, b: 70, l: 80 };
    var cw = W - pad.l - pad.r;
    var ch = H - pad.t - pad.b;

    var allV = visTemps.concat(visSPs);
    var yMin = Math.min.apply(null, allV);
    var yMax = Math.max.apply(null, allV);
    var yPad = (yMax - yMin) * 0.15 + 5;
    yMin = Math.max(0, yMin - yPad);
    yMax = yMax + yPad;
    var yRng = yMax - yMin || 1;
    
    var xMin = visTS[0];
    var xMax = visTS[visTS.length - 1];
    var xRng = xMax - xMin || 1;

    function px(x) { return pad.l + ((x - xMin) / xRng) * cw; }
    function py(y) { return pad.t + ch - ((y - yMin) / yRng) * ch; }

    // --- Header & Legend Rendering ---
    ctxOff.fillStyle = "#2c3e50";
    ctxOff.font = "bold 28px Arial, sans-serif";
    ctxOff.textAlign = "left";
    ctxOff.textBaseline = "middle";
    
    var exportTitle = (LANG && LANG.title) ? LANG.title : "ReflowOven";
    var textX = pad.l;
    var headerCenterY = pad.t - 30; // Centered in the 60px space above the graph

    if (iconImg) {
      ctxOff.drawImage(iconImg, pad.l, pad.t - 56, 40, 53);
      textX += 55; // Shift title right to clear image
    }
    
    ctxOff.fillText(exportTitle, textX, headerCenterY);

    var titleWidth = ctxOff.measureText(exportTitle).width;
    var legendX = textX + titleWidth + 40;

    ctxOff.font = "16px Arial, sans-serif";
    
    // Temperature Legend
    ctxOff.fillStyle = "#3498db";
    ctxOff.fillRect(legendX, headerCenterY - 3, 30, 6);
    ctxOff.fillStyle = "#34495e";
    ctxOff.fillText("Air Temp", legendX + 40, headerCenterY);
    
    // Setpoint Legend
    ctxOff.fillStyle = "#e74c3c";
    ctxOff.fillRect(legendX + 130, headerCenterY - 3, 30, 6);
    ctxOff.fillStyle = "#34495e";
    ctxOff.fillText("Setpoint", legendX + 170, headerCenterY);

    // Part Legend
    ctxOff.fillStyle = "#2ecc71";
    ctxOff.fillRect(legendX + 260, headerCenterY - 3, 30, 6);
    ctxOff.fillStyle = "#34495e";
    ctxOff.fillText("Part Temp", legendX + 300, headerCenterY);

    // --- ADDED: PID Settings Overlay ---
    var kp = document.getElementById("pidKp").value || "--";
    var ki = document.getElementById("pidKi").value || "--";
    var kd = document.getElementById("pidKd").value || "--";
    var pidText = "PID: P=" + kp + "  I=" + ki + "  D=" + kd;

    ctxOff.textAlign = "right";
    ctxOff.fillStyle = "#7f8c8d";
    ctxOff.font = "italic 16px Arial, sans-serif";
    ctxOff.fillText(pidText, W - pad.r, headerCenterY);

    // --- Y-Axis ---
    ctxOff.strokeStyle = "#ecf0f1";
    ctxOff.lineWidth = 1;
    ctxOff.fillStyle = "#7f8c8d";
    ctxOff.font = "14px Arial, sans-serif";
    ctxOff.textAlign = "right";
    ctxOff.textBaseline = "middle";

    var ySteps = 10;
    for (var i = 0; i <= ySteps; i++) {
      var yv = yMin + (yRng / ySteps) * i;
      var yp = py(yv);
      ctxOff.beginPath();
      ctxOff.moveTo(pad.l, yp);
      ctxOff.lineTo(pad.l + cw, yp);
      ctxOff.stroke();
      ctxOff.fillText(Math.round(yv), pad.l - 15, yp);
    }

    // --- X-Axis (UPDATED to hh:mm:ss format) ---
    var xSteps = 12;
    ctxOff.textAlign = "center";
    ctxOff.textBaseline = "top";
    for (var j = 0; j <= xSteps; j++) {
      var xv = Math.max(0, Math.round(xMin + (xRng / xSteps) * j));
      var xp = px(xv);
      
      // Calculate HH:MM:SS
      var h = Math.floor(xv / 3600);
      var m = Math.floor((xv % 3600) / 60);
      var s = xv % 60;
      var timeStr = ("0" + h).slice(-2) + ":" + ("0" + m).slice(-2) + ":" + ("0" + s).slice(-2);
      
      ctxOff.beginPath();
      ctxOff.moveTo(xp, pad.t);
      ctxOff.lineTo(xp, pad.t + ch);
      ctxOff.stroke();
      ctxOff.fillText(timeStr, xp, pad.t + ch + 15);
    }

    // Chart Border
    ctxOff.strokeStyle = "#bdc3c7";
    ctxOff.lineWidth = 1.5;
    ctxOff.strokeRect(pad.l, pad.t, cw, ch);

    // Axis Labels (UPDATED Label to Time (hh:mm:ss))
    ctxOff.fillStyle = "#2c3e50";
    ctxOff.font = "italic 16px Arial, sans-serif";
    ctxOff.textAlign = "center";
    ctxOff.save();
    ctxOff.translate(pad.l - 55, pad.t + ch / 2);
    ctxOff.rotate(-Math.PI / 2);
    ctxOff.fillText("Temps (\u00B0C)", 0, 0); 
    ctxOff.restore();
    ctxOff.fillText("Time (hh:mm:ss)", pad.l + cw / 2, pad.t + ch + 45);

    // --- Line Drawing ---
    // Setpoints
    ctxOff.strokeStyle = "#e74c3c";
    ctxOff.lineWidth = 3;
    ctxOff.beginPath();
    for (var k = 0; k < visSPs.length; k++) {
      if (k === 0) ctxOff.moveTo(px(visTS[k]), py(visSPs[k]));
      else ctxOff.lineTo(px(visTS[k]), py(visSPs[k]));
    }
    ctxOff.stroke();

    // Actual Temperatures
    ctxOff.strokeStyle = "#3498db";
    ctxOff.lineWidth = 3;
    ctxOff.lineJoin = "round";
    ctxOff.beginPath();
    for (var n = 0; n < visTemps.length; n++) {
      if (n === 0) ctxOff.moveTo(px(visTS[n]), py(visTemps[n]));
      else ctxOff.lineTo(px(visTS[n]), py(visTemps[n]));
    }
    ctxOff.stroke();

    // Part Temperatures (TC2)
    ctxOff.strokeStyle = "#2ecc71";
    ctxOff.lineWidth = 3;
    ctxOff.lineJoin = "round";
    ctxOff.beginPath();
    var t2ExpActive = false;
    for (var n = 0; n < visTemps2.length; n++) {
      if (visTemps2[n] > 0.1) {
        if (!t2ExpActive) { ctxOff.moveTo(px(visTS[n]), py(visTemps2[n])); t2ExpActive = true; }
        else { ctxOff.lineTo(px(visTS[n]), py(visTemps2[n])); }
      } else {
        t2ExpActive = false;
      }
    }
    ctxOff.stroke();

    // --- Download Handling ---
    if (offCanvas.toBlob) {
      offCanvas.toBlob(function(blob) {
        var url = URL.createObjectURL(blob);
        var a = document.createElement("a");
        a.href = url;
        var dStr = new Date().toISOString().slice(0, 10);
        a.download = "reflow_graph_" + dStr + ".png";
        
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        
        setTimeout(function() { URL.revokeObjectURL(url); }, 200);
      }, "image/png");
    } else {
      var dataUrl = offCanvas.toDataURL("image/png");
      var a2 = document.createElement("a");
      a2.href = dataUrl;
      var dStr2 = new Date().toISOString().slice(0, 10);
      a2.download = "reflow_graph_" + dStr2 + ".png";
      document.body.appendChild(a2);
      a2.click();
      document.body.removeChild(a2);
    }
  }
}

// --- INITIALIZATION ---
initI18n();
window.addEventListener("resize", drawChart);
scheduleStatus(1000);
scheduleHistory(1000); 
setTimeout(drawChart, 200);
loadProfiles();

fetch("/getcustom")
  .then(r => r.json())
  .then(d => {
    if (!d || typeof d !== 'object') return;
    if (d.name) document.getElementById("custName").value = String(d.name).slice(0,31);
    if (d.steps && Array.isArray(d.steps) && d.steps.length > 0) {
      document.getElementById("stepList").innerHTML = "";
      for (var i = 0; i < d.steps.length; i++) {
        var s = d.steps[i];
        addCustomStep(String(s.label || LANG.step_def).slice(0,23), validateNumber(s.temp, 0, 280, 100), validateNumber(s.hold, 1, 600, 10));
      }
    }
  }).catch(() => {});
)rawliteral";