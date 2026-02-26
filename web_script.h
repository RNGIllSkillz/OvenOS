#pragma once
#include <Arduino.h>

const char PAGE_JS[] PROGMEM = R"rawliteral(
const AUTH_TOKEN = "oven2026";
var CUSTOM_PROF_IDX = 999;
var profileCache = [];

var mode = "manual";
var prevMsg = "";
var prevStep = -1;
var histTemps = [], histSPs = [], histTS = [];
var liveTemps = [], liveSPs = [], liveTS = [];
var liveStartWall = 0;
var LIVE_MAX = 3600; 
var statusPending = false;
var historyPending = false;
var statusTimer = null;
var historyTimer = null;
var chartScale = 600;
var canvas = document.getElementById("chart");
var ctx = canvas.getContext("2d");
var isSettingsOpen = false; 

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
  return str.replace(/["'\\\/\x00-\x1f]/g, "").trim().slice(0, 31) || "Без имени";
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
  document.querySelectorAll(".sbtn").forEach(function(b) {
    b.classList.toggle("on", +b.dataset.val === chartScale);
  });
  drawChart();
}

function drawChart() {
  var container = canvas.parentElement;
  var W = container.clientWidth - 44;
  var H = container.clientHeight - 50; 
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

  var mergedTemps = [], mergedSPs = [], mergedTS = [];
  var liveOffset = liveTS.length > 0 ? liveTS[0] : Infinity;
  
  for (var mi = 0; mi < histTS.length; mi++) {
    if (histTS[mi] < liveOffset - 1) { 
      mergedTemps.push(histTemps[mi]);
      mergedSPs.push(histSPs[mi]);
      mergedTS.push(histTS[mi]);
    }
  }
  
  for (var li = 0; li < liveTS.length; li++) {
    mergedTemps.push(liveTemps[li]);
    mergedSPs.push(liveSPs[li]);
    mergedTS.push(liveTS[li]);
  }

  var visTemps, visSPs, visTS;
  var allTS = mergedTS;
  
  if (allTS.length === 0) {
    ctx.fillStyle = "#4a5058"; 
    ctx.font = "13px 'Share Tech Mono'";
    ctx.textAlign = "center";
    ctx.fillText("Ожидание данных...", W/2, H/2);
    return;
  }
  
  if (chartScale === 0) {
    visTemps = mergedTemps; visSPs = mergedSPs; visTS = mergedTS;
  } else {
    var tCutoff = allTS[allTS.length-1] - chartScale;
    var startIdx = 0;
    for (var si = 0; si < allTS.length; si++) {
      if (allTS[si] >= tCutoff) { startIdx = si; break; }
    }
    visTemps = mergedTemps.slice(startIdx);
    visSPs = mergedSPs.slice(startIdx);
    visTS = mergedTS.slice(startIdx);
  }

  if (visTS.length < 2) return;

  var allV = visTemps.concat(visSPs);
  var yMin = Math.min.apply(null, allV), yMax = Math.max.apply(null, allV);
  var yPad = (yMax - yMin) * 0.15 + 5;
  yMin = Math.max(0, yMin - yPad); 
  yMax = yMax + yPad;
  var yRng = yMax - yMin || 1;
  var xMax = visTS[visTS.length-1];
  var xMin = chartScale > 0 ? Math.max(0, xMax - chartScale) : visTS[0];
  var xRng = xMax - xMin || 1;

  function px(x) { return pad.l + (x - xMin) / xRng * cw; }
  function py(y) { return pad.t + ch - (y - yMin) / yRng * ch; }

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

  ctx.strokeStyle = "#3ab0e8"; ctx.lineWidth = 1.5; ctx.setLineDash([6,4]);
  ctx.beginPath();
  for (var k = 0; k < visSPs.length; k++) {
    if (k===0) ctx.moveTo(px(visTS[k]), py(visSPs[k]));
    else ctx.lineTo(px(visTS[k]), py(visSPs[k]));
  }
  ctx.stroke(); 
  ctx.setLineDash([]);

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

  ctx.strokeStyle = "#e8900a"; ctx.lineWidth = 2;
  ctx.beginPath();
  for (var p2 = 0; p2 < visTemps.length; p2++) {
    if (p2===0) ctx.moveTo(px(visTS[p2]), py(visTemps[p2]));
    else ctx.lineTo(px(visTS[p2]), py(visTemps[p2]));
  }
  ctx.stroke();
  ctx.restore();

  ctx.fillStyle="#e8900a"; ctx.fillRect(pad.l,H-14,16,3);
  ctx.fillStyle="#d4cfc9"; ctx.font="10px 'Share Tech Mono'"; ctx.textAlign="left";
  ctx.fillText("Темп.", pad.l+20, H-10);
  ctx.strokeStyle="#3ab0e8"; ctx.lineWidth=1.5; ctx.setLineDash([4,3]);
  ctx.beginPath(); ctx.moveTo(pad.l+68,H-12); ctx.lineTo(pad.l+84,H-12); ctx.stroke();
  ctx.setLineDash([]); ctx.fillStyle="#d4cfc9";
  ctx.fillText("Цель", pad.l+88, H-10);
}

// --- PROFILE LOADING ---

function loadProfiles() {
  var sel = document.getElementById("profSel");
  sel.innerHTML = "<option>Загрузка...</option>";
  sel.disabled = true;

  fetch("/profiles")
    .then(r => r.json())
    .then(data => {
      profileCache = data; 
      sel.innerHTML = "";
      
      data.forEach((p, index) => {
        var opt = document.createElement("option");
        opt.value = index;
        opt.textContent = p.name;
        sel.appendChild(opt);
      });
      
      sel.disabled = false;
      CUSTOM_PROF_IDX = data.length; 
      renderPreview(); 
    })
    .catch(e => {
      sel.innerHTML = "<option>Ошибка загрузки</option>";
      addLog("Ошибка профилей: "+e, "err");
    });
}

function renderPreview() {
  var idx = parseInt(document.getElementById("profSel").value);
  if (isNaN(idx) || idx < 0 || !profileCache[idx]) return;
  
  var steps = profileCache[idx].steps;
  var h = "";
  for (var i=0; i<steps.length; i++) {
    h += "<div class='prow'><span class='pname'>"+escapeHtml(steps[i].l)+"</span><span>"+steps[i].t+"C / "+steps[i].h+"мин</span></div>";
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
    title.textContent = "Настройки PID";
  } else {
    main.classList.remove("hidden");
    sets.classList.remove("on");
    title.textContent = "Модель управления";
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
    addCustomStep("Преднагрев", 150, 30);
    addCustomStep("Охлаждение", 25, 10);
  }
}

var stepCounter = 0;
function addCustomStep(label, temp, hold) {
  label = label || "Шаг"; 
  temp = validateNumber(temp, 0, 280, 100); 
  hold = validateNumber(hold, 1, 600, 10);
  
  var id = "step_" + (stepCounter++);
  var div = document.createElement("div");
  div.className = "step-row"; 
  div.id = id;
  
  var inLabel = document.createElement("input"); 
  inLabel.type="text"; inLabel.value=label; inLabel.maxLength=23; inLabel.placeholder="Название";
  var inTemp = document.createElement("input"); 
  inTemp.type="number"; inTemp.value=temp; inTemp.min=0; inTemp.max=280; inTemp.placeholder="°C";
  var inHold = document.createElement("input"); 
  inHold.type="number"; inHold.value=hold; inHold.min=1; inHold.max=600; inHold.placeholder="мин";
  var btnDel = document.createElement("button"); 
  btnDel.className="step-del"; btnDel.textContent="×";
  btnDel.onclick = function(){ var el = document.getElementById(id); if(el) el.remove(); };
  
  div.appendChild(inLabel); div.appendChild(inTemp); div.appendChild(inHold); div.appendChild(btnDel);
  document.getElementById("stepList").appendChild(div);
}

function getCustomSteps() {
  var rows = document.getElementById("stepList").querySelectorAll(".step-row");
  var steps = [];
  for (var i=0; i<rows.length; i++) {
    var inputs = rows[i].querySelectorAll("input");
    if (inputs.length < 3) continue;
    var label = sanitize(inputs[0].value) || "Step";
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
  fd.append("token", AUTH_TOKEN);
  
  setRunning(true);
  histTemps = []; histSPs = []; histTS = [];
  liveTemps = []; liveSPs = []; liveTS = [];
  liveStartWall = Date.now();
  drawChart();

  if (mode === "manual") {
    var temp = validateNumber(document.getElementById("tTemp").value, 0, 280, 100);
    var time = validateNumber(document.getElementById("tDur").value, 1, 600, 10);
    fd.append("mode","manual"); fd.append("temp", temp); fd.append("time", time);
    addLog("Запуск: Ручной режим "+temp+"°C / "+time+"мин", "ev");
    
    fetch("/start",{method:"POST",body:fd})
      .then(r => { if(r.status === 409) return r.text().then(t => { throw new Error(t); }); if(!r.ok) throw new Error("HTTP "+r.status); })
      .catch(e => { setRunning(false); addLog("Ошибка запуска: "+e, "err"); });

  } else if (mode === "profile") {
    var pi = parseInt(document.getElementById("profSel").value);
    if (isNaN(pi) || pi < 0 || pi >= profileCache.length) {
         addLog("Неверный профиль", "err"); setRunning(false); return;
    }
    fd.append("mode","profile"); fd.append("profile", pi);
    addLog("Запуск: "+profileCache[pi].name, "ev");
    fetch("/start",{method:"POST",body:fd})
      .then(r => { if(!r.ok) return r.text().then(t => { throw new Error(t); }); })
      .catch(e => { setRunning(false); addLog("Ошибка запуска: "+e, "err"); });

  } else {
    var steps = getCustomSteps();
    if (steps.length === 0) { alert("Добавьте хотя бы один шаг."); setRunning(false); return; }
    var name = sanitize(document.getElementById("custName").value) || "Свой";
    document.getElementById("custName").value = name;
    var profJson = JSON.stringify({name:name, steps:steps});
    addLog("Запуск: Свой '"+name+"'", "ev");
    
    fetch("/setcustom",{method:"POST",headers:{"Content-Type":"application/json"},body:profJson})
    .then(r => {
      if(!r.ok) throw new Error("Save failed");
      var fd2 = new FormData(); fd2.append("token", AUTH_TOKEN);
      fd2.append("mode","profile"); fd2.append("profile", String(CUSTOM_PROF_IDX));
      return fetch("/start",{method:"POST",body:fd2});
    })
    .then(r => { if(!r.ok) return r.text().then(t => { throw new Error(t); }); })
    .catch(e => { setRunning(false); addLog("Ошибка: "+e, "err"); });
  }
}

function saveCustomProfile() {
  var steps = getCustomSteps();
  if (steps.length === 0) { alert("Сначала добавьте хотя бы один шаг."); return; }
  var btn = document.querySelector(".bsave");
  btn.textContent = "Сохранение..."; btn.disabled = true;
  var name = sanitize(document.getElementById("custName").value) || "Custom";
  document.getElementById("custName").value = name;
  var profJson = JSON.stringify({name:name, steps:steps});
  
  fetch("/setcustom",{method:"POST",headers:{"Content-Type":"application/json"},body:profJson})
  .then(r => {
    if (!r.ok) throw new Error("Save failed");
    btn.innerHTML = "&#10003; Сохранено!"; btn.classList.add("saved"); btn.disabled = false;
    addLog("Профиль сохранён: '"+name+"'", "success");
    setTimeout(() => { btn.innerHTML = "&#10003; Сохранить профиль"; btn.classList.remove("saved"); }, 2500);
  })
  .catch(() => {
    btn.textContent = "Ошибка сохранения"; btn.disabled = false;
    addLog("Ошибка сохранения профиля", "err");
    setTimeout(() => { btn.innerHTML = "&#10003; Сохранить профиль"; }, 2500);
  });
}

function savePID() {
  var fd = new FormData();
  fd.append("token", AUTH_TOKEN);
  fd.append("kp", document.getElementById("pidKp").value);
  fd.append("ki", document.getElementById("pidKi").value);
  fd.append("kd", document.getElementById("pidKd").value);
  
  fetch("/setpid", {method:"POST", body:fd})
    .then(r => {
       if(r.ok) { addLog("PID сохранен", "success"); toggleSettings(); }
       else addLog("Ошибка сохранения PID", "err");
    });
}

function doStop() {
  var fd = new FormData();
  fd.append("token", AUTH_TOKEN);
  
  fetch("/stop",{method:"POST",body:fd})
    .then(r => {
        if (!r.ok) throw new Error("HTTP "+r.status);
        setRunning(false);
        addLog("Остановка выполнена", "warn");
    })
    .catch(e => {
        addLog("ОШИБКА ОСТАНОВКИ: " + e, "err");
    });
}

function doReset() {
  var fd = new FormData(); fd.append("token", AUTH_TOKEN);
  fetch("/reset",{method:"POST",body:fd})
  .then(r => { if(r.ok) addLog("Сброс ошибки выполнен", "success"); else addLog("Ошибка сброса", "err"); });
}

// --- POLLING ---

var failCount = 0;
function scheduleStatus(delay) {
  clearTimeout(statusTimer);
  statusTimer = setTimeout(pollStatus, Math.max(1500, delay)); 
}

function pollStatus() {
  if (statusPending) { scheduleStatus(2500); return; }
  statusPending = true;
  var t0 = Date.now();
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), 3000); 

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
      wb.textContent = "WiFi: " + rssi + "dBm";
      wb.className = "wifi-badge";
      if (rssi > -60) wb.classList.add("wifi-good");
      else if (rssi > -75) wb.classList.add("wifi-ok");
      else wb.classList.add("wifi-bad");

      if (d.running) { btnGear.disabled = true; if(isSettingsOpen) toggleSettings(); } 
      else { btnGear.disabled = false; }

      if (d.emergency) {
          badge.textContent = "! АВАРИЯ !"; badge.className = "crit";
          btnStart.style.display = "none"; btnStop.style.display = "none"; btnReset.style.display = "block";
      } else {
          badge.textContent = "* Онлайн"; badge.className = "live";
          btnStart.style.display = "block"; btnStop.style.display = "block"; btnReset.style.display = "none";
      }

      if (d.monitoring) {
          btnMon.classList.add("mon-active");
          btnMon.style.borderColor = "var(--blu)";
          btnMon.style.color = "var(--blu)";
          btnMon.innerHTML = "&#9632; СТОП";
      } else {
          btnMon.classList.remove("mon-active");
          btnMon.style.borderColor = "var(--bdr)";
          btnMon.style.color = "var(--mut)";
          btnMon.innerHTML = "&#128200; МОНИТОР";
      }
      btnMon.disabled = d.running;

      var temp = validateNumber(d.temp, -50, 500, 0);
      var el = document.getElementById("curT");
      el.textContent = temp.toFixed(1);
      el.className = "tbig "+(temp>150?"hot":temp<40?"cool":"");

      if (d.running) {
        if (liveStartWall === 0) liveStartWall = Date.now() - validateNumber(d.elapsed, 0, 86400000, 0) * 1000;
        if (liveStartWall === 0) liveStartWall = Date.now();
        var liveNow = (Date.now() - liveStartWall) / 1000;
        if (liveTS.length === 0 || liveNow - liveTS[liveTS.length-1] >= 1) {
           liveTemps.push(temp); liveSPs.push(validateNumber(d.setpoint, 0, 300, 0)); liveTS.push(liveNow);
           if (liveTS.length > LIVE_MAX) { liveTemps.shift(); liveSPs.shift(); liveTS.shift(); }
           drawChart();
        }
      }

      var sp = d.setpoint ? validateNumber(d.setpoint, 0, 300, 0) : 0;
      document.getElementById("spV").textContent = sp ? sp.toFixed(0) : "---";

      var pill = document.getElementById("pill");
      var msg = String(d.msg || "Неизвестно");
      pill.textContent = msg; pill.className = "pill";
      if (/НАГРЕВ|СТАБИЛ|Сушка|Отжиг|Оплавление|Выдержка|Преднагрев|Пред/.test(msg)) pill.classList.add("heat");
      else if (/ПОДДЕРЖКА|ЗАВЕРШЕН|КОНЕЦ/.test(msg)) pill.classList.add("hold");
      else if (/АВАРИЯ|ОСТАНОВКА|TIMEOUT|FAULT|SPIKE|OVERHEAT|ПАДЕНИЕ/.test(msg)) pill.classList.add("err");

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
        sn.textContent = d.profName;
        sb.textContent = "Шаг "+(d.profStep+1)+" из "+d.profSteps+" — "+msg;
        var dh = "";
        for (var i=0; i<d.profSteps; i++) {
          var dc = i<d.profStep?"done":(i===d.profStep?"active":"");
          dh += "<div class='dot "+dc+"'></div>";
        }
        ds.innerHTML = dh;
        if (d.profStep !== prevStep) { addLog("Шаг "+(d.profStep+1)+"/"+d.profSteps+": "+msg, "ev"); prevStep = d.profStep; }
      } else {
        sn.textContent = d.running?"Ручной режим":"--"; sb.textContent=""; ds.innerHTML="";
      }

      if (msg !== prevMsg) { 
        var logCls = "ev";
        if (/АВАРИЯ|TIMEOUT|FAULT|SPIKE|OVERHEAT|ПАДЕНИЕ/.test(msg)) logCls = "err";
        else if (/ПОДДЕРЖКА|ЗАВЕРШЕН|КОНЕЦ|ГОТОВО/.test(msg)) logCls = "success";
        addLog("Состояние: "+msg, logCls); prevMsg = msg; 
      }
      
      if (d.kp !== undefined) {
         if (document.activeElement.id !== "pidKp") document.getElementById("pidKp").value = d.kp;
         if (document.activeElement.id !== "pidKi") document.getElementById("pidKi").value = d.ki;
         if (document.activeElement.id !== "pidKd") document.getElementById("pidKd").value = d.kd;
      }
      var elapsed = Date.now() - t0;
      scheduleStatus(elapsed > 800 ? 2000 : 1000);
    })
    .catch(() => {
      statusPending = false; failCount++;
      if (failCount > 3) { document.getElementById("badge").textContent = "* ПОМЕРЛО"; document.getElementById("badge").className = ""; }
      scheduleStatus(3000);
    });
}

function scheduleHistory(delay) {
  clearTimeout(historyTimer);
  historyTimer = setTimeout(pollHistory, delay);
}

function toggleMonitor() {
  // Determine current state based on button class or data, but easier to just ask for toggle logic
  // We'll rely on the visual state to decide what to send, or store a var.
  // Best approach: Read the button's current active state.
  var btn = document.getElementById("btnMon");
  var isMon = btn.classList.contains("mon-active");
  var newState = isMon ? "0" : "1";

  var fd = new FormData();
  fd.append("token", AUTH_TOKEN);
  fd.append("state", newState);

  fetch("/monitor", {method:"POST", body:fd})
    .then(r => {
        if(r.ok) {
            // Optimistic update
            if(newState==="1") {
                addLog("Мониторинг запущен", "ev");
                // Reset chart vars locally to match server reset
                histTemps=[]; histSPs=[]; histTS=[]; liveTemps=[]; liveSPs=[]; liveTS=[];
                liveStartWall = Date.now();
            } else {
                addLog("Мониторинг остановлен", "ev");
            }
        }
    });
}

function pollHistory() {
  if (historyPending) { scheduleHistory(10000); return; }
  historyPending = true;
  fetch("/history")
    .then(r => r.json())
    .then(d => {
      historyPending = false;
      if (d.ts && Array.isArray(d.ts) && d.ts.length > 0) {
        var cutoff = liveTS.length > 0 ? liveTS[0] : Infinity;
        histTemps = []; histSPs = []; histTS = [];
        for (var hi = 0; hi < d.ts.length; hi++) {
          if (d.ts[hi] < cutoff || liveTS.length === 0) {
            histTemps.push(validateNumber(d.temps[hi], -50, 500, 0));
            histSPs.push(validateNumber(d.sps[hi], 0, 300, 0));
            histTS.push(validateNumber(d.ts[hi], 0, 86400000, 0));
          }
        }
        drawChart();
      }
      scheduleHistory(60000);
    })
    .catch(() => { historyPending = false; scheduleHistory(60000); });
}

// --- INITIALIZATION ---
window.addEventListener("resize", drawChart);
scheduleStatus(1000);
scheduleHistory(5000);
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
        addCustomStep(String(s.label || "Шаг").slice(0,23), validateNumber(s.temp, 0, 280, 100), validateNumber(s.hold, 1, 600, 10));
      }
    }
  }).catch(() => {});
)rawliteral";