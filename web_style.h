#pragma once
#include <Arduino.h>

const char PAGE_CSS[] PROGMEM = R"rawliteral(

/* CSS CUSTOM PROPERTIES  (design tokens) */

:root {
  /* Surfaces */
  --bg:   #0d0f10;
  --sur:  #161a1d;
  --bdr:  #252a2e;

  /* Accent colours */
  --amb:  #e8900a;
  --amb2: #ffb84d;
  --red:  #e84040;
  --grn:  #2ecc71;
  --blu:  #3ab0e8;

  /* Text */
  --txt: #d4cfc9;
  --mut: #4a5058;

  /* Fonts */
  --mono: "Share Tech Mono", monospace;
  --sans: "Barlow Condensed", sans-serif;
}


/* RESET & BASE */

*, *::before, *::after {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

html, body {
  height: 100%;
}

body {
  background:     var(--bg);
  color:          var(--txt);
  font-family:    var(--sans);
  min-height:     100vh;
  display:        flex;
  flex-direction: column;
}


/* HEADER */

header {
  display:         flex;
  align-items:     center;
  justify-content: space-between;
  padding:         12px 24px;
  background:      var(--sur);
  border-bottom:   1px solid var(--bdr);
  flex-shrink:     0;
}

.logo {
  font-size:      1.2rem;
  font-weight:    700;
  letter-spacing: .15em;
  color:          var(--amb);
}

.logo em {
  color:       var(--txt);
  font-style:  normal;
  font-weight: 300;
}


/* Status badge */

#badge {
  font-family:   var(--mono);
  font-size:     .72rem;
  padding:       2px 10px;
  border-radius: 2px;
  background:    #1a1d20;
  border:        1px solid var(--bdr);
  color:         var(--mut);
  white-space:   nowrap;
  flex-shrink:   0;
  transition:    all .4s;
}

#badge.live { color: var(--grn); border-color: var(--grn); }
#badge.crit { color: var(--red); border-color: var(--red); animation: blink 1s infinite; }


/* Wi-Fi badge */

.wifi-badge {
  font-family:   var(--mono);
  font-size:     .72rem;
  padding:       2px 10px;
  border-radius: 2px;
  background:    #1a1d20;
  border:        1px solid var(--bdr);
  color:         var(--mut);
  white-space:   nowrap;
  flex-shrink:   0;
  transition:    all .4s;
}

.wifi-good { color: var(--grn); border-color: var(--grn); }
.wifi-ok   { color: var(--amb); border-color: var(--amb); }
.wifi-bad  { color: var(--red); border-color: var(--red); animation: blink 2s infinite; }

/* Language select */

#langSel {
  font-family:   var(--mono);
  font-size:     .72rem;
  padding:       1px 10px;
  border-radius: 2px;
  background:    #1a1d20;
  border:        1px solid var(--bdr);
  color:         var(--mut);
  white-space:   nowrap;
  flex-shrink:   0;
  cursor:        pointer;
  outline:       none;
  transition:    all .4s;
  width:         62px;
  height:        22px;
  line-height:   18px;
}

/* Animations */

@keyframes blink {
  50% { opacity: .5; }
}

@keyframes fadeIn {
  from { opacity: 0; transform: translateY(-5px); }
  to   { opacity: 1; transform: translateY(0);    }
}


/* LAYOUT GRID */

.grid {
  display:               grid;
  grid-template-columns: 290px 1fr;
  grid-template-rows:    1fr auto;
  flex:                  1;
  min-height:            0;
  background:            var(--bdr);
  gap:                   1px;
}

.left {
  display:        flex;
  flex-direction: column;
  gap:            1px;
  background:     var(--bdr);
  grid-row:       1 / -1;
  overflow-y:     auto;
}

.right {
  display:        flex;
  flex-direction: column;
  gap:            1px;
  background:     var(--bdr);
  min-height:     0;
}


/* GENERIC PANE / LABEL */

.pane {
  background: var(--sur);
  padding:    20px 22px;
}

.plabel {
  font-size:      .6rem;
  letter-spacing: .2em;
  text-transform: uppercase;
  color:          var(--mut);
  margin-bottom:  14px;
  padding-bottom: 8px;
  border-bottom:  1px solid var(--bdr);
}


/* TEMPERATURE PANE */

.temp-pane {
  text-align: center;
  padding:    26px 22px 18px;
}

.tbig {
  font-family:  var(--mono);
  font-size:    4.8rem;
  line-height:  1;
  color:        var(--amb);
  text-shadow:  0 0 40px rgba(232, 144, 10, .3);
  transition:   color .5s;
}

.tbig.cool { color: var(--blu); text-shadow: 0 0 40px rgba(58,  176, 232, .25); }
.tbig.hot  { color: var(--red); text-shadow: 0 0 40px rgba(232,  64,  64, .35); }

.tunit {
  font-size:      1.8rem;
  color:          var(--mut);
  vertical-align: super;
}

.sprow {
  margin-top:  6px;
  font-family: var(--mono);
  font-size:   .8rem;
  color:       var(--mut);
}

.sprow span { color: var(--amb2); }


/* Status pill */

.pill {
  margin-top:     12px;
  display:        inline-block;
  padding:        4px 14px;
  border-radius:  2px;
  background:     #1a1e22;
  border:         1px solid var(--bdr);
  font-family:    var(--mono);
  font-size:      .72rem;
  letter-spacing: .1em;
  transition:     all .4s;
}

.pill.heat { border-color: var(--amb); color: var(--amb); }
.pill.hold { border-color: var(--grn); color: var(--grn); }
.pill.err  { border-color: var(--red); color: var(--red); background: rgba(232, 64, 64, 0.1); }


/* TIMER PANE */

.timer-pane {
  text-align: center;
  padding:    14px 22px;
}

.tlabel {
  font-size:      .6rem;
  letter-spacing: .2em;
  text-transform: uppercase;
  color:          var(--mut);
}

#timer {
  font-family: var(--mono);
  font-size:   2.6rem;
  color:       var(--txt);
  margin-top:  3px;
}

.pbar-wrap {
  height:        3px;
  background:    var(--bdr);
  margin-top:    8px;
  border-radius: 2px;
  overflow:      hidden;
}

#pbar {
  height:        100%;
  width:         0%;
  background:    linear-gradient(90deg, var(--amb), var(--amb2));
  transition:    width 1s linear;
  border-radius: 2px;
}


/* STEP PANE */

.step-pane {
  padding:         12px 22px;
  background:      var(--sur);
  font-family:     var(--mono);
  font-size:       .74rem;
  color:           var(--mut);
  min-height:      52px;
  display:         flex;
  flex-direction:  column;
  justify-content: center;
}

.step-name {
  font-size:     .88rem;
  color:         var(--txt);
  margin-bottom: 2px;
}

.dots {
  display:    flex;
  gap:        5px;
  margin-top: 7px;
}

.dot {
  width:         8px;
  height:        8px;
  border-radius: 50%;
  background:    var(--bdr);
  transition:    background .3s;
}

.dot.done   { background: var(--mut); }
.dot.active { background: var(--amb); box-shadow: 0 0 6px var(--amb); }


/* CONTROL PANE */

.ctrl-pane {
  flex:       1;
  padding:    18px 22px;
  overflow-y: auto;
  position:   relative;
}

/* Ctrl pane header row */

.ctrl-header {
  display:         flex;
  justify-content: space-between;
  align-items:     center;
  border-bottom:   1px solid var(--bdr);
  margin-bottom:   14px;
  padding-bottom:  8px;
}

.ctrl-header .plabel {
  margin:  0;
  padding: 0;
  border:  none;
}

.gear-btn {
  background:  transparent;
  border:      none;
  font-size:   1.2rem;
  color:       var(--mut);
  cursor:      pointer;
  transition:  color 0.3s;
}

.gear-btn:hover    { color: var(--amb); }
.gear-btn:disabled { color: var(--bdr); cursor: not-allowed; }

#settingsPane         { display: none; }
#mainControls.hidden  { display: none; }
#settingsPane.on      { display: block; animation: fadeIn 0.3s; }


/* Tabs */

.tabs {
  display:       flex;
  border:        1px solid var(--bdr);
  border-radius: 3px;
  overflow:      hidden;
  margin-bottom: 18px;
}

.tab {
  flex:           1;
  padding:        7px;
  background:     transparent;
  border:         none;
  color:          var(--mut);
  font-family:    var(--sans);
  font-size:      .82rem;
  font-weight:    600;
  letter-spacing: .1em;
  text-transform: uppercase;
  cursor:         pointer;
  transition:     all .2s;
}

.tab.on { background: var(--amb); color: #000; }

.cgroup    { display: none; }
.cgroup.on { display: block; }


/* Form rows */

.frow {
  display:         flex;
  align-items:     center;
  justify-content: space-between;
  margin-bottom:   12px;
}

.flabel {
  font-size:      .7rem;
  text-transform: uppercase;
  letter-spacing: .12em;
  color:          var(--mut);
}

input[type=number],
input[type=text],
select {
  background:    #0d0f10;
  border:        1px solid var(--bdr);
  color:         var(--txt);
  font-family:   var(--mono);
  font-size:     .9rem;
  padding:       5px 9px;
  border-radius: 3px;
  outline:       none;
  transition:    border-color .2s;
}

input[type=number] { width: 100px; }
input[type=text]   { width: 100%; }

select {
  width:         100%;
  margin-bottom: 12px;
}

input:focus,
select:focus { border-color: var(--amb); }


/* Profile preview */

.prof-preview {
  font-family:   var(--mono);
  font-size:     .7rem;
  color:         var(--mut);
  border:        1px solid var(--bdr);
  border-radius: 3px;
  overflow:      hidden;
  margin-bottom: 12px;
}

.prow {
  padding:       5px 9px;
  display:       flex;
  justify-content: space-between;
  border-bottom: 1px solid var(--bdr);
}

.prow:last-child { border-bottom: none; }
.prow .pname     { color: var(--txt); }


/* BUTTONS */

.btnrow {
  display:    flex;
  gap:        8px;
  margin-top: 16px;
}

.btn {
  flex:           1;
  padding:        13px 6px;
  border:         none;
  border-radius:  3px;
  font-family:    var(--sans);
  font-size:      .95rem;
  font-weight:    700;
  letter-spacing: .12em;
  text-transform: uppercase;
  cursor:         pointer;
  transition:     opacity .2s, transform .1s;
}

.btn:active   { transform: scale(.97); }
.btn:disabled { opacity: .4; cursor: not-allowed; transform: none; }

.bstart  { background: var(--grn); color: #000; }
.bstop   { background: var(--red); color: #fff; }
.breset  { background: var(--red); color: #fff; border: 1px solid #fff; display: none; }

.bsm     { padding: 6px 10px; font-size: .75rem; flex: none; width: auto; }
.bcustom { background: #1e2427; color: var(--txt); border: 1px solid var(--bdr); }

.bsave {
  background:    transparent;
  border:        1px solid var(--amb);
  color:         var(--amb);
  margin-bottom: 10px;
  transition:    background .2s;
}

.bsave:hover  { background: rgba(232, 144, 10, 0.12); }
.bsave.saved  { border-color: var(--grn); color: var(--grn); }


/* CUSTOM PROFILE / STEP EDITOR */

.custom-name-row {
  display:       flex;
  align-items:   center;
  gap:           8px;
  margin-bottom: 12px;
}

.custom-name-row label {
  font-size:      .7rem;
  text-transform: uppercase;
  letter-spacing: .1em;
  color:          var(--mut);
  white-space:    nowrap;
}

.step-list {
  display:        flex;
  flex-direction: column;
  gap:            6px;
  margin-bottom:  10px;
}

.step-row {
  display:               grid;
  grid-template-columns: 1fr 70px 60px 28px;
  gap:                   5px;
  align-items:           center;
  background:            #0d0f10;
  border:                1px solid var(--bdr);
  border-radius:         3px;
  padding:               6px 8px;
}

.step-row input[type=text],
.step-row input[type=number] {
  font-size: .78rem;
  padding:   3px 6px;
}

.step-row input[type=number] { width: 100%; }

.step-del {
  background:    transparent;
  border:        1px solid var(--bdr);
  color:         var(--mut);
  border-radius: 3px;
  cursor:        pointer;
  font-size:     1rem;
  line-height:   1;
  padding:       2px 6px;
  transition:    all .2s;
}

.step-del:hover { border-color: var(--red); color: var(--red); }

.step-hdrs {
  display:               grid;
  grid-template-columns: 1fr 70px 60px 28px;
  gap:                   5px;
  padding:               0 8px;
  margin-bottom:         3px;
}

.step-hdrs span {
  font-size:      .6rem;
  text-transform: uppercase;
  letter-spacing: .1em;
  color:          var(--mut);
}

.add-step-btn {
  width:          100%;
  padding:        7px;
  background:     transparent;
  border:         1px dashed var(--bdr);
  color:          var(--mut);
  font-family:    var(--sans);
  font-size:      .8rem;
  letter-spacing: .1em;
  text-transform: uppercase;
  cursor:         pointer;
  border-radius:  3px;
  transition:     all .2s;
  margin-bottom:  10px;
}

.add-step-btn:hover { border-color: var(--amb); color: var(--amb); }


/* CHART PANE */

.chart-pane {
  background:     var(--sur);
  padding:        20px 22px;
  flex:           1;
  display:        flex;
  flex-direction: column;
  min-height:     0;
}

canvas {
  display: block;
  width:   100%;
  flex:    1;
}

.chart-header {
  display:         flex;
  align-items:     center;
  justify-content: space-between;
  margin-bottom:   14px;
  padding-bottom:  8px;
  border-bottom:   1px solid var(--bdr);
  flex-shrink:     0;
}

.scale-btns { display: flex; gap: 4px; }

.sbtn {
  padding:       3px 10px;
  background:    transparent;
  border:        1px solid var(--bdr);
  color:         var(--mut);
  font-family:   var(--mono);
  font-size:     .7rem;
  border-radius: 2px;
  cursor:        pointer;
  transition:    all .2s;
}

.sbtn:hover { border-color: var(--amb); color: var(--amb); }
.sbtn.on    { background: var(--amb); border-color: var(--amb); color: #000; }

.gmode.on { background: #252a2e; border-color: #252a2e; color: var(--txt); }


/* LOG PANE */

.log-pane {
  background: var(--sur);
  padding:    18px 22px;
}

.logbox {
  font-family: var(--mono);
  font-size:   .7rem;
  color:       var(--mut);
  max-height:  120px;
  overflow-y:  auto;
}

.le {
  padding:       2px 0;
  border-bottom: 1px solid #1a1e21;
}

.le .ts      { color: #2a2f33; margin-right: 8px; }
.le.ev       { color: var(--amb2); }
.le.warn     { color: var(--amb);  }
.le.err      { color: var(--red);  }
.le.success  { color: var(--grn);  }


/* RESPONSIVE  (≤ 700 px) */

@media (max-width: 700px) {
  .grid {
    grid-template-columns: 1fr;
    grid-template-rows:    auto 1fr auto;
  }

  .left  { grid-row: auto; }
  .tbig  { font-size: 3.4rem; }
}

)rawliteral";