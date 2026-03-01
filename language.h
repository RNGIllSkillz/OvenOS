#pragma once
#include <Arduino.h>

// ==============================================================================
//   BACKEND TRANSLATIONS (C++)
//   These are now English-only. They act as "keys" sent to the browser.
//   The browser will translate these dynamically into the user's chosen language.
// ==============================================================================

// General States
#define L_STATE_WAIT          "WAITING"
#define L_STATE_HEATING       "HEATING..."
#define L_STATE_STABLE        "STABILIZING"
#define L_STATE_HOLD          "HOLD"
#define L_STATE_DONE          "DONE"
#define L_STATE_END           "END"
#define L_STATE_STOPPED       "STOPPED"

// Errors (Prefix must match exactly for alarm logic detection)
#define L_ERR_PREFIX          "ALARM: " 
#define L_ERR_FAULT           "FAULT"
#define L_ERR_INVALID_STEP    "INVALID STEP"
#define L_ERR_TC_FAIL         "THERMOCOUPLE ERROR"
#define L_ERR_MAX_TEMP        "MAX TEMP LIMIT"
#define L_ERR_OVERSHOOT       "OVERSHOOT (>SP+20)"
#define L_ERR_DROP            "TEMP DROP"
#define L_ERR_TIMEOUT         "TIMEOUT"
#define L_ERR_RUNAWAY         "THERMAL RUNAWAY"

// Default Built-in Profiles & Steps
#define L_PROF_SOLDER         "Reflow Soldering"
#define L_PROF_ANNEAL         "ABS Annealing"
#define L_PROF_PLA            "PLA / TPU / PVA"
#define L_PROF_PETG           "PETG / PET +CF/GF"
#define L_PROF_ABS            "ABS / ASA +CF/GF"
#define L_PROF_PA             "PA / Nylon +CF/GF"
#define L_PROF_PC             "PC / PC-ABS"
#define L_PROF_PP             "PP +CF/GF"
#define L_PROF_PPS            "PPS +CF/GF"
#define L_PROF_PEEK           "PEEK +CF/GF"

#define L_STEP_PREHEAT        "Preheat"
#define L_STEP_SOAK           "Soak"
#define L_STEP_REFLOW         "Reflow"
#define L_STEP_COOL           "Cooling"
#define L_STEP_ANNEAL         "Anneal"
#define L_STEP_DRY            "Drying"
#define L_STEP_PREDRY         "Pre-dry"

#define L_DEFAULT_CUSTOM      "Custom Profile"
#define L_DEFAULT_STEP        "Step"

// ==============================================================================
//   FRONTEND TRANSLATIONS (JavaScript / HTML UI)
// ==============================================================================

const char WEB_LANG_JS[] PROGMEM = R"rawliteral(
const I18N = {
  en: {
    title: "CGF Oven",
    subtitle: "Temperature Controller",
    badge_offline: "OFFLINE",
    badge_online: "ONLINE",
    badge_alarm: "! ALARM !",
    temp_label: "Temperature",
    target_prefix: "Target &nbsp;",
    target_idle: "---",
    pill_idle: "Idle",
    btn_monitor: "&#128200; MONITOR",
    btn_monitor_stop: "&#9632; STOP",
    timer_label: "Current Step Time Left",
    ctrl_title: "Control Mode",
    ctrl_settings: "PID Settings",
    tab_manual: "Manual",
    tab_profile: "Profile",
    tab_custom: "Custom",
    man_target: "Target (&deg;C)",
    man_dur: "Hold (min)",
    prof_sel: "Select Profile",
    prof_loading: "Loading...",
    prof_err: "Load Error",
    cust_name: "Name:",
    cust_hdr_name: "NAME",
    cust_hdr_temp: "TEMP &deg;C",
    cust_hdr_hold: "HOLD MIN",
    btn_add_step: "+ ADD STEP",
    btn_save: "&#10003; Save",
    btn_saving: "Saving...",
    btn_saved: "&#10003; Saved!",
    btn_save_err: "Save Error",
    step_placeholder: "Step Name",
    btn_start: "START",
    btn_stop: "STOP",
    btn_reset: "RESET FAULT",
    btn_save_pid: "SAVE PID",
    btn_back: "BACK",
    chart_title: "Chart",
    chart_live: "Live",
    chart_hist: "History",
    chart_sync: "&#8635; Sync",
    chart_10m: "10m",
    chart_30m: "30m",
    chart_1h: "1h",
    chart_all: "All",
    chart_wait_live: "Waiting for stream...",
    chart_wait_hist: "Waiting for history...",
    chart_lbl_temp: "Temp",
    chart_lbl_target: "Target",
    log_title: "Event Log",
    msg_sync_empty: "History is empty. Cannot sync.",
    msg_sync_added: "Added ",
    msg_sync_pts: " points from history to Live",
    msg_sync_skip: "Live chart already contains all history data",
    msg_start_man: "Start: Manual mode ",
    msg_start_prof: "Start: ",
    msg_start_err: "Start error: ",
    msg_prof_invalid: "Invalid profile",
    msg_prof_req_step: "Add at least one step.",
    msg_prof_saved: "Profile saved: ",
    msg_prof_save_err: "Profile save error",
    msg_pid_ok: "PID saved",
    msg_pid_err: "Error saving PID",
    msg_stop_ok: "Stopped successfully",
    msg_stop_err: "STOP ERROR: ",
    msg_reset_ok: "Fault reset",
    msg_reset_err: "Reset error",
    msg_dead: "* DEAD",
    msg_mon_start: "Monitoring started",
    msg_mon_stop: "Monitoring stopped",
    msg_step: "Step ",
    msg_of: " of ",
    msg_state: "State: ",
    msg_manual_mode: "Manual Mode",
    msg_unknown: "Unknown",
    def_step_heat: "Preheat",
    def_step_cool: "Cooling",
    def_cust_name: "Custom Profile",
    step_def: "Step",
    lbl_min: "min",
    chart_export: "&#128247; Export",
    msg_export_err: "Not enough data to export.",

    // This block translates ESP32 string output to the UI
    backend: {
      "WAITING": "Waiting",
      "HEATING...": "Heating...",
      "STABILIZING": "Stabilizing",
      "HOLD": "Hold",
      "DONE": "Done",
      "END": "End",
      "STOPPED": "Stopped",
      "ALARM: ": "ALARM: ",
      "FAULT": "Fault",
      "INVALID STEP": "Invalid Step",
      "THERMOCOUPLE ERROR": "Thermocouple Error",
      "MAX TEMP LIMIT": "Max Temp Limit Reached",
      "OVERSHOOT (>SP+20)": "Overshoot (>SP+20)",
      "TEMP DROP": "Temp Dropped",
      "TIMEOUT": "Timeout",
      "THERMAL RUNAWAY": "Thermal Runaway Detected",
      "Reflow Soldering": "Reflow Soldering",
      "ABS Annealing": "ABS Annealing",
      "PLA / TPU / PVA": "PLA / TPU / PVA",
      "PETG / PET +CF/GF": "PETG / PET +CF/GF",
      "ABS / ASA +CF/GF": "ABS / ASA +CF/GF",
      "PA / Nylon +CF/GF": "PA / Nylon +CF/GF",
      "PC / PC-ABS": "PC / PC-ABS",
      "PP +CF/GF": "PP +CF/GF",
      "PPS +CF/GF": "PPS +CF/GF",
      "PEEK +CF/GF": "PEEK +CF/GF",
      "Preheat": "Preheat",
      "Soak": "Soak",
      "Reflow": "Reflow",
      "Cooling": "Cooling",
      "Anneal": "Anneal",
      "Drying": "Drying",
      "Pre-dry": "Pre-dry",
      "Custom Profile": "Custom Profile",
      "Step": "Step"
    }
  },
  ru: {
    title: "CGFЦЭХ",
    subtitle: "Контроллер температуры",
    badge_offline: "ОФФЛАЙН",
    badge_online: "Онлайн",
    badge_alarm: "! АВАРИЯ !",
    temp_label: "Температура",
    target_prefix: "Цель &nbsp;",
    target_idle: "---",
    pill_idle: "Бездействие",
    btn_monitor: "&#128200; МОНИТОР",
    btn_monitor_stop: "&#9632; СТОП",
    timer_label: "Оставшееся время текущего шага",
    ctrl_title: "Модель управления",
    ctrl_settings: "Настройки PID",
    tab_manual: "Ручная",
    tab_profile: "Профиль",
    tab_custom: "Свой",
    man_target: "Цель (&deg;C)",
    man_dur: "Удержание (мин)",
    prof_sel: "Выбор профиля",
    prof_loading: "Загрузка...",
    prof_err: "Ошибка загрузки",
    cust_name: "Имя:",
    cust_hdr_name: "ИМЯ",
    cust_hdr_temp: "ТЕМП. °C",
    cust_hdr_hold: "ВЫДЕРЖКА МИН",
    btn_add_step: "+ ДОБАВИТЬ ШАГ",
    btn_save: "&#10003; Сохранить",
    btn_saving: "Сохранение...",
    btn_saved: "&#10003; Сохранено!",
    btn_save_err: "Ошибка сохранения",
    step_placeholder: "Название",
    btn_start: "СТАРТ",
    btn_stop: "СТОП",
    btn_reset: "СБРОСИТЬ ОШИБКУ",
    btn_save_pid: "СОХРАНИТЬ PID",
    btn_back: "НАЗАД",
    chart_title: "График",
    chart_live: "Live",
    chart_hist: "Архив",
    chart_sync: "&#8635; Sync",
    chart_10m: "10м",
    chart_30m: "30м",
    chart_1h: "1ч",
    chart_all: "Всё",
    chart_wait_live: "Ожидание потока...",
    chart_wait_hist: "Ожидание архива...",
    chart_lbl_temp: "Темп.",
    chart_lbl_target: "Цель",
    log_title: "Журнал событий",
    msg_sync_empty: "Архив пуст. Синхронизация невозможна.",
    msg_sync_added: "Добавлено ",
    msg_sync_pts: " точек из архива в Live",
    msg_sync_skip: "Live график уже содержит все архивные данные",
    msg_start_man: "Запуск: Ручной режим ",
    msg_start_prof: "Запуск: ",
    msg_start_err: "Ошибка запуска: ",
    msg_prof_invalid: "Неверный профиль",
    msg_prof_req_step: "Добавьте хотя бы один шаг.",
    msg_prof_saved: "Профиль сохранён: ",
    msg_prof_save_err: "Ошибка сохранения профиля",
    msg_pid_ok: "PID сохранен",
    msg_pid_err: "Ошибка сохранения PID",
    msg_stop_ok: "Остановка выполнена",
    msg_stop_err: "ОШИБКА ОСТАНОВКИ: ",
    msg_reset_ok: "Сброс ошибки выполнен",
    msg_reset_err: "Ошибка сброса",
    msg_dead: "* ПОМЕРЛО",
    msg_mon_start: "Мониторинг запущен",
    msg_mon_stop: "Мониторинг остановлен",
    msg_step: "Шаг ",
    msg_of: " из ",
    msg_state: "Состояние: ",
    msg_manual_mode: "Ручной режим",
    msg_unknown: "Неизвестно",
    def_step_heat: "Преднагрев",
    def_step_cool: "Охлаждение",
    def_cust_name: "Свой профиль",
    step_def: "Шаг",
    lbl_min: "мин",
    chart_export: "&#128247; Экспорт",
    msg_export_err: "Нет данных для экспорта.",

    backend: {
      "WAITING": "ОЖИДАНИЕ",
      "HEATING...": "НАГРЕВ...",
      "STABILIZING": "СТАБИЛИЗАЦИЯ",
      "HOLD": "ПОДДЕРЖКА",
      "DONE": "ГОТОВО",
      "END": "КОНЕЦ",
      "STOPPED": "ОСТАНОВЛЕНО",
      "ALARM: ": "АВАРИЯ: ",
      "FAULT": "СБОЙ",
      "INVALID STEP": "НЕВЕРНЫЙ ШАГ",
      "THERMOCOUPLE ERROR": "ТЕРМОПАРА",
      "MAX TEMP LIMIT": "ПРЕВЫШЕН ЛИМИТ ТЕМП.",
      "OVERSHOOT (>SP+20)": "ПЕРЕГРЕВ (>SP+20)",
      "TEMP DROP": "ПАДЕНИЕ ТЕМП.",
      "TIMEOUT": "ТАЙМАУТ",
      "THERMAL RUNAWAY": "ТЕПЛОВОЙ РАЗГОН",
      "Reflow Soldering": "Пайка оплавлением",
      "ABS Annealing": "Отжиг ABS",
      "PLA / TPU / PVA": "PLA / TPU / PVA",
      "PETG / PET +CF/GF": "PETG / PET +CF/GF",
      "ABS / ASA +CF/GF": "ABS / ASA +CF/GF",
      "PA / Nylon +CF/GF": "PA / Nylon +CF/GF",
      "PC / PC-ABS": "PC / PC-ABS",
      "PP +CF/GF": "PP +CF/GF",
      "PPS +CF/GF": "PPS +CF/GF",
      "PEEK +CF/GF": "PEEK +CF/GF",
      "Preheat": "Преднагрев",
      "Soak": "Выдержка",
      "Reflow": "Оплавление",
      "Cooling": "Охлаждение",
      "Anneal": "Отжиг",
      "Drying": "Сушка",
      "Pre-dry": "Пред. сушка",
      "Custom Profile": "Свой профиль",
      "Step": "Шаг"
    }
  }
};
const userLang = localStorage.getItem('oven_lang') || 'en';
const LANG = I18N[userLang] || I18N['en'];
)rawliteral";