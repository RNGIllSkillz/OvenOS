#pragma once
#include <Arduino.h>

const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CGF Oven</title>
<script src="lang.js"></script>
<link rel="stylesheet" href="style.css">
</head>
<body>

<header>
  <div class="logo"><span data-i18n="title">CGF Oven</span> &middot; <em data-i18n="subtitle">Temperature Controller</em></div>
  <div style="display:flex; gap:10px; align-items:center;">
  
    <select id="langSel" onchange="changeLang()" style="background:#161a1d; color:var(--mut); border:1px solid var(--bdr); border-radius:3px; padding:2px 4px; font-size:0.75rem; height:24px; outline:none; cursor:pointer;">
      <option value="en">EN</option>
      <option value="ru">RU</option>
    </select>

    <div id="wifi-badge" class="wifi-badge">WiFi: --</div>

    <div id="badge">&#9679; <span data-i18n="badge_offline">OFFLINE</span></div>
  </div>
</header>

<div class="grid">
  <div class="left">
    <div class="pane temp-pane">
      <div class="plabel" data-i18n="temp_label">Temperature</div>
      <div><span id="curT" class="tbig">---</span><span class="tunit">&deg;C</span></div>
      <div class="sprow"><span data-i18n="target_prefix">Target &nbsp;</span><span id="spV" data-i18n="target_idle">---</span> &deg;C</div>
      <div><span id="pill" class="pill" data-i18n="pill_idle">Idle</span></div>
      <button id="btnMon" class="btn bsm" style="margin-top:12px; background:#1e2427; border:1px solid var(--bdr); color:var(--mut);" onclick="toggleMonitor()" data-i18n="btn_monitor">
        &#128200; MONITOR
      </button>
    </div>

    <div class="pane timer-pane">
      <div class="tlabel" data-i18n="timer_label">Current Step Time Left</div>
      <div id="timer">--:--:--</div>
      <div class="pbar-wrap"><div id="pbar"></div></div>
    </div>

    <div id="stepPane" class="step-pane">
      <div id="stepName" class="step-name">&mdash;</div>
      <div id="stepSub"></div>
      <div class="dots" id="dots"></div>
    </div>

    <div class="pane ctrl-pane">
      <div class="ctrl-header">
        <div class="plabel" id="ctrlTitle" data-i18n="ctrl_title">Control Mode</div>
        <button id="btnGear" class="gear-btn" onclick="toggleSettings()">&#9881;</button>
      </div>

      <!-- MAIN CONTROLS -->
      <div id="mainControls">
        <div class="tabs">
          <button class="tab on" onclick="switchMode('manual')" data-i18n="tab_manual">Manual</button>
          <button class="tab" onclick="switchMode('profile')" data-i18n="tab_profile">Profile</button>
          <button class="tab" onclick="switchMode('custom')" data-i18n="tab_custom">Custom</button>
        </div>

        <div id="manGrp" class="cgroup on">
          <div class="frow">
            <span class="flabel" data-i18n="man_target">Target (&deg;C)</span>
            <input type="number" id="tTemp" value="100" min="0" max="280">
          </div>
          <div class="frow">
            <span class="flabel" data-i18n="man_dur">Hold (min)</span>
            <input type="number" id="tDur" value="10" min="1" max="600">
          </div>
        </div>

        <div id="profGrp" class="cgroup">
          <div class="flabel" style="margin-bottom:8px" data-i18n="prof_sel">Select Profile</div>
          <select id="profSel" onchange="renderPreview()"><option data-i18n="prof_loading">Loading...</option></select>
          <div id="profPreview" class="prof-preview"></div>
        </div>

        <div id="customGrp" class="cgroup">
          <div class="custom-name-row">
            <label data-i18n="cust_name">Name:</label>
            <input type="text" id="custName" maxlength="31">
          </div>
          <div class="step-hdrs">
            <span data-i18n="cust_hdr_name">NAME</span><span data-i18n="cust_hdr_temp">TEMP °C</span><span data-i18n="cust_hdr_hold">HOLD MIN</span><span></span>
          </div>
          <div class="step-list" id="stepList"></div>
          <button class="add-step-btn" onclick="addCustomStep()" data-i18n="btn_add_step">+ ADD STEP</button>
          <button class="btn bcustom bsave" onclick="saveCustomProfile()" data-i18n="btn_save">&#10003; Save</button>
        </div>

        <div class="btnrow">
          <button class="btn bstart" id="btnStart" onclick="doStart()" data-i18n="btn_start">START</button>
          <button class="btn bstop" id="btnStop" onclick="doStop()" data-i18n="btn_stop">STOP</button>
          <button class="btn breset" id="btnReset" onclick="doReset()" data-i18n="btn_reset">RESET FAULT</button>
        </div>
      </div>

      <!-- SETTINGS PANE (HIDDEN) -->
      <div id="settingsPane">
         <div class="frow">
            <span class="flabel">Kp (Proportional)</span>
            <input type="number" id="pidKp" step="0.1">
          </div>
          <div class="frow">
            <span class="flabel">Ki (Integral)</span>
            <input type="number" id="pidKi" step="0.01">
          </div>
          <div class="frow">
            <span class="flabel">Kd (Derivative)</span>
            <input type="number" id="pidKd" step="0.01">
          </div>
          <div class="btnrow">
            <button class="btn bcustom" onclick="savePID()" data-i18n="btn_save_pid">SAVE PID</button>
            <button class="btn bcustom" onclick="toggleSettings()" style="border-color:var(--bdr); color:var(--mut)" data-i18n="btn_back">BACK</button>
          </div>
      </div>

    </div>
  </div>

  <div class="right">
    <div class="pane chart-pane">
      <div class="chart-header">
        <div style="display:flex; align-items:center; gap:12px;">
          <span class="plabel" style="margin:0;padding:0;border:none" data-i18n="chart_title">Chart</span>
          <div class="scale-btns">
            <button class="sbtn gmode on" id="btnGLive" onclick="setGraphMode('live')" data-i18n="chart_live">Live</button>
            <button class="sbtn gmode" id="btnGHist" onclick="setGraphMode('hist')" data-i18n="chart_hist">History</button>
            <button class="sbtn gmode" onclick="syncLiveGraph()" data-i18n="chart_sync">&#8635; Sync</button>
            <button class="sbtn gmode" onclick="exportChart()" data-i18n="chart_export">&#128247; Export</button>
          </div>
        </div>
        <div class="scale-btns">
          <button class="sbtn tscale on" onclick="setScale(600)" data-val="600" data-i18n="chart_10m">10m</button>
          <button class="sbtn tscale" onclick="setScale(1800)" data-val="1800" data-i18n="chart_30m">30m</button>
          <button class="sbtn tscale" onclick="setScale(3600)" data-val="3600" data-i18n="chart_1h">1h</button>
          <button class="sbtn tscale" onclick="setScale(0)" data-val="0" data-i18n="chart_all">All</button>
        </div>
      </div>
      <div id="canvasWrap" style="flex:1; position:relative; min-height:0; width:100%;">
        <canvas id="chart" style="position:absolute; top:0; left:0; width:100%; height:100%;"></canvas>
      </div>
    </div>

    <div class="pane log-pane">
      <div class="plabel" data-i18n="log_title">Event Log</div>
      <div class="logbox" id="log"></div>
    </div>
  </div>
</div>
<script src="script.js"></script>
</body>
</html>
)rawliteral";