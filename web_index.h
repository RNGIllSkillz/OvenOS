#pragma once
#include <Arduino.h>

const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CGFЦЭХ</title>
<link rel="stylesheet" href="style.css">
</head>
<body>
<header>
  <div class="logo">CGFЦЭХ  &middot; <em>Контроллер температуры</em></div>
  <div style="display:flex; gap:10px;">
    <div id="wifi-badge" class="wifi-badge">WiFi: --</div>
    <div id="badge">&#9679; OFFLINE</div>
  </div>
</header>
<div class="grid">
  <div class="left">
    <div class="pane temp-pane">
      <div class="plabel">Температура</div>
      <div><span id="curT" class="tbig">---</span><span class="tunit">&deg;C</span></div>
      <div class="sprow">Цель &nbsp;<span id="spV">---</span> &deg;C</div>
      <div><span id="pill" class="pill">Бездействие</span></div>
      <button id="btnMon" class="btn bsm" style="margin-top:12px; background:#1e2427; border:1px solid var(--bdr); color:var(--mut);" onclick="toggleMonitor()">
        &#128200; МОНИТОР
      </button>
    </div>

    <div class="pane timer-pane">
      <div class="tlabel">Оставшееся время текущего шага</div>
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
        <div class="plabel" id="ctrlTitle">Модель управления</div>
        <button id="btnGear" class="gear-btn" onclick="toggleSettings()" title="Настройки">&#9881;</button>
      </div>

      <!-- MAIN CONTROLS -->
      <div id="mainControls">
        <div class="tabs">
          <button class="tab on" onclick="switchMode('manual')">Ручная</button>
          <button class="tab" onclick="switchMode('profile')">Профиль</button>
          <button class="tab" onclick="switchMode('custom')">Свой</button>
        </div>

        <div id="manGrp" class="cgroup on">
          <div class="frow">
            <span class="flabel">Цель (&deg;C)</span>
            <input type="number" id="tTemp" value="100" min="0" max="280">
          </div>
          <div class="frow">
            <span class="flabel">Удержание (мин)</span>
            <input type="number" id="tDur" value="10" min="1" max="600">
          </div>
        </div>

        <div id="profGrp" class="cgroup">
          <div class="flabel" style="margin-bottom:8px">Выбор профиля</div>
          <select id="profSel" onchange="renderPreview()"></select>
          <div id="profPreview" class="prof-preview"></div>
        </div>

        <div id="customGrp" class="cgroup">
          <div class="custom-name-row">
            <label>Имя:</label>
            <input type="text" id="custName" value="Свой профиль" maxlength="31">
          </div>
          <div class="step-hdrs">
            <span>ИМЯ</span><span>ТЕМП. °C</span><span>ВЫДЕРЖКА МИН</span><span></span>
          </div>
          <div class="step-list" id="stepList"></div>
          <button class="add-step-btn" onclick="addCustomStep()">+ ДОБАВИТЬ ШАГ</button>
          <button class="btn bcustom bsave" onclick="saveCustomProfile()">&#10003; Сохранить</button>
        </div>

        <div class="btnrow">
          <button class="btn bstart" id="btnStart" onclick="doStart()">СТАРТ</button>
          <button class="btn bstop" id="btnStop" onclick="doStop()">СТОП</button>
          <button class="btn breset" id="btnReset" onclick="doReset()">СБРОСИТЬ ОШИБКУ</button>
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
            <button class="btn bcustom" onclick="savePID()">СОХРАНИТЬ PID</button>
            <button class="btn bcustom" onclick="toggleSettings()" style="border-color:var(--bdr); color:var(--mut)">НАЗАД</button>
          </div>
      </div>

    </div>
  </div>

  <div class="right">
    <div class="pane chart-pane">
      <div class="chart-header">
        <span class="plabel" style="margin:0;padding:0;border:none">График температуры</span>
        <div class="scale-btns">
          <button class="sbtn on" onclick="setScale(600)" data-val="600">10м</button>
          <button class="sbtn" onclick="setScale(1800)" data-val="1800">30м</button>
          <button class="sbtn" onclick="setScale(3600)" data-val="3600">1ч</button>
          <button class="sbtn" onclick="setScale(0)" data-val="0">Всё</button>
        </div>
      </div>
      <canvas id="chart" height="220"></canvas>
    </div>

    <div class="pane log-pane">
      <div class="plabel">Журнал событий</div>
      <div class="logbox" id="log"></div>
    </div>
  </div>
</div>
<script src="script.js"></script>
</body>
</html>
)rawliteral";