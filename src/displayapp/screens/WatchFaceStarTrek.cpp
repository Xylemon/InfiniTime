#include "displayapp/screens/WatchFaceStarTrek.h"

#include <lvgl/lvgl.h>
#include <cstdio>
#include "displayapp/screens/BatteryIcon.h"
#include "displayapp/screens/BleIcon.h"
#include "displayapp/screens/NotificationIcon.h"
#include "displayapp/screens/Symbols.h"
#include "components/battery/BatteryController.h"
#include "components/ble/BleController.h"
#include "components/ble/NotificationManager.h"
#include "components/heartrate/HeartRateController.h"
#include "components/motion/MotionController.h"
#include "components/ble/weather/WeatherService.h"
#include "components/settings/Settings.h"
#include "displayapp/icons/watchfacestartrek/bracket_left.c"
#include "displayapp/icons/watchfacestartrek/bracket_right.c"
using namespace Pinetime::Applications::Screens;
using namespace Pinetime::Controllers;

// ########## Color scheme
constexpr lv_color_t COLOR_LIGHTBLUE = LV_COLOR_MAKE(0x83, 0xdd, 0xff);
constexpr lv_color_t COLOR_DARKBLUE = LV_COLOR_MAKE(0x09, 0x46, 0xee);
constexpr lv_color_t COLOR_BLUE = LV_COLOR_MAKE(0x37, 0x86, 0xff);
constexpr lv_color_t COLOR_ORANGE = LV_COLOR_MAKE(0xd4, 0x5f, 0x10);
constexpr lv_color_t COLOR_DARKGRAY = LV_COLOR_MAKE(0x48, 0x60, 0x6c);
constexpr lv_color_t COLOR_LIGHTGRAY = LV_COLOR_MAKE(0xdd, 0xdd, 0xdd);
constexpr lv_color_t COLOR_BEIGE = LV_COLOR_MAKE(0xad, 0xa8, 0x8b);
constexpr lv_color_t COLOR_BROWN = LV_COLOR_MAKE(0x64, 0x44, 0x00);
constexpr lv_color_t COLOR_BLACK = LV_COLOR_MAKE(0x00, 0x00, 0x00);
constexpr lv_color_t COLOR_WHITE = LV_COLOR_MAKE(0xff, 0xff, 0xff);

constexpr lv_color_t COLOR_TIME = COLOR_LIGHTBLUE;
constexpr lv_color_t COLOR_DATE = LV_COLOR_MAKE(0x22, 0x22, 0x22);
constexpr lv_color_t COLOR_ICONS = LV_COLOR_MAKE(0x11, 0x11, 0x11);
constexpr lv_color_t COLOR_HEARTBEAT_ON = LV_COLOR_MAKE(0xff, 0x4f, 0x10);
constexpr lv_color_t COLOR_HEARTBEAT_OFF = COLOR_BLUE;
constexpr lv_color_t COLOR_STEPS = COLOR_BEIGE;
constexpr lv_color_t COLOR_BG = COLOR_BLACK;

// ########### Config strings
constexpr const char* WANT_SYSTEM_FONT = "System font";
constexpr const char* WANT_ST_FONT = "Star Trek font";
constexpr const char* WANT_ST_FONT_BUT_NO = "Not installed";
constexpr const char* WANT_STATIC = "Static";
constexpr const char* WANT_ANIMATE_START = "Startup";
constexpr const char* WANT_ANIMATE_CONTINUOUS = "Cont.";
constexpr const char* WANT_ANIMATE_ALL = "All";
constexpr const char* WANT_WEATHER = "Weather";
constexpr const char* WANT_NO_WEATHER = "No Weather";
constexpr const char* WANT_SECONDS = "Seconds";
constexpr const char* WANT_MINUTES = "Minutes";

// ########## Timing constants
constexpr uint16_t SETTINGS_AUTO_CLOSE_TICKS = 5000;
constexpr uint16_t ANIMATOR_START_TICKS = 50;
constexpr uint16_t ANIMATOR_CONTINUOUS_TICKS = 500;

// ########## Touch handler
namespace {
  void event_handler(lv_obj_t* obj, lv_event_t event) {
    auto* screen = static_cast<WatchFaceStarTrek*>(obj->user_data);
    screen->UpdateSelected(obj, event);
  }
}

WatchFaceStarTrek::WatchFaceStarTrek(Controllers::DateTime& dateTimeController,
                                     const Controllers::Battery& batteryController,
                                     const Controllers::Ble& bleController,
                                     Controllers::NotificationManager& notificatioManager,
                                     Controllers::Settings& settingsController,
                                     Controllers::HeartRateController& heartRateController,
                                     Controllers::MotionController& motionController,
                                     Controllers::FS& filesystem,
                                     Controllers::WeatherService& weatherService)
  : dateTimeController {dateTimeController},
    batteryController {batteryController},
    bleController {bleController},
    notificatioManager {notificatioManager},
    settingsController {settingsController},
    heartRateController {heartRateController},
    motionController {motionController},
    filesystem {filesystem},
    weatherService {weatherService},
    currentDateTime {{}},
    batteryIcon(false) {

  lfs_file f = {};
  if (filesystem.FileOpen(&f, "/fonts/edge_of_the_galaxy.bin", LFS_O_RDONLY) >= 0) {
    filesystem.FileClose(&f);
    starTrekFontAvailable = true;
  }
  if (settingsController.GetStarTrekUseSystemFont()) {
    font_time = &jetbrains_mono_extrabold_compressed;
  } else {
    if (starTrekFontAvailable) {
      font_time = lv_font_load("F:/fonts/edge_of_the_galaxy.bin");
    } else {
      font_time = &jetbrains_mono_extrabold_compressed;
    }
  }

  animatorContinuousTick = lv_tick_get();
  Settings::StarTrekAnimateType animateType = settingsController.GetStarTrekAnimate();
  if (animateType == Settings::StarTrekAnimateType::All || animateType == Settings::StarTrekAnimateType::Start) {
    drawWatchFace(false);
    startStartAnimation();
  } else {
    drawWatchFace();
  }

  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  Refresh();
}

WatchFaceStarTrek::~WatchFaceStarTrek() {
  lv_task_del(taskRefresh);

  if (starTrekFontAvailable && !settingsController.GetStarTrekUseSystemFont() && font_time != nullptr) {
    lv_font_free(font_time);
  }

  lv_obj_clean(lv_scr_act());
}

// ########## Watch face drawing ###############################################

void WatchFaceStarTrek::drawWatchFace(bool visible) {
  // definitions of gaps and sizes
  // with the future vision to make this all canvas size independent :)
  // -- list for date and stuff
  constexpr uint8_t gap = 3;
  constexpr uint8_t cellheight = 26;
  constexpr uint8_t cellwidth = 72;
  constexpr uint8_t cells_x = 34;
  constexpr uint8_t cpg = cellheight + gap;
  // -- end of upper stuff (all magic numbers at the moment)
  constexpr uint8_t upperend = 80; // the upper arch shape portion ends at this y
  constexpr uint8_t upg = upperend + gap;
  // -- icon spaces
  constexpr uint8_t iconrectwidth = 17; // icon space consists of a rect and a circ
  constexpr uint8_t iconrect_x = 14;
  constexpr uint8_t icon_x = 0;
  // -- decorative bars
  constexpr uint8_t bar_y = upperend - cellheight;
  constexpr uint8_t barwidth = 4;
  constexpr uint8_t bargap = 7;
  // -- precomputed distances
  constexpr uint8_t listdistance2 = upg + cpg;
  constexpr uint8_t listdistance3 = upg + 2 * cpg;
  constexpr uint8_t listdistance4 = upg + 3 * cpg;
  constexpr uint8_t bar1_x = cells_x - bargap;
  constexpr uint8_t bar2_x = cells_x - 2 * bargap;
  // -- end of list part (back to magic numbers below this)
  constexpr uint8_t listend = upperend + 4 * cellheight + 5 * gap;

  topRightRect = rect(visible, 84, 11, 156, 0, COLOR_DARKGRAY);
  upperShape[0] = rect(visible, 85, 11, 68, 0, COLOR_BLUE);
  upperShape[1] = rect(visible, 72, 46, 34, 34, COLOR_BLUE);
  upperShape[2] = rect(visible, 36, 80, 70, 0, COLOR_BLUE);
  upperShape[3] = rect(visible, 14, 14, 106, 11, COLOR_BLUE);
  upperShape[4] = circ(visible, 68, 34, 0, COLOR_BLUE);
  upperShape[5] = circ(visible, 28, 106, 11, COLOR_BG);
  lowerShape[0] = circ(visible, 68, 34, 172, COLOR_BLUE);   // draw these two first, because circle is to big
  lowerShape[1] = rect(visible, 68, 34, 34, 172, COLOR_BG); // and has to be occluded by this
  lowerShape[2] = rect(visible, 85, 11, 68, 229, COLOR_BLUE);
  lowerShape[3] = rect(visible, 72, 240 - listend - 34, 34, listend, COLOR_BLUE);
  lowerShape[4] = rect(visible, 36, 240 - listend, 70, listend, COLOR_BLUE);
  lowerShape[5] = rect(visible, 14, 14, 106, 215, COLOR_BLUE);
  lowerShape[6] = circ(visible, 28, 106, 201, COLOR_BG);
  bottomRightRect = rect(visible, 84, 11, 156, 229, COLOR_ORANGE);

  listItem1[0] = rect(visible, cellwidth, cellheight, cells_x, upg, COLOR_BEIGE);
  listItem2[0] = rect(visible, cellwidth, cellheight, cells_x, listdistance2, COLOR_ORANGE);
  listItem3[0] = rect(visible, cellwidth, cellheight, cells_x, listdistance3, COLOR_LIGHTBLUE);
  listItem4[0] = rect(visible, cellwidth, cellheight, cells_x, listdistance4, COLOR_DARKGRAY);
  listItem1[1] = rect(visible, iconrectwidth, cellheight, iconrect_x, upg, COLOR_LIGHTBLUE);
  listItem2[1] = rect(visible, iconrectwidth, cellheight, iconrect_x, listdistance2, COLOR_BEIGE);
  listItem3[1] = rect(visible, iconrectwidth, cellheight, iconrect_x, listdistance3, COLOR_BROWN);
  listItem4[1] = rect(visible, iconrectwidth, cellheight, iconrect_x, listdistance4, COLOR_DARKBLUE);
  listItem1[2] = circ(visible, cellheight, icon_x, upg, COLOR_LIGHTBLUE);
  listItem2[2] = circ(visible, cellheight, icon_x, listdistance2, COLOR_BEIGE);
  listItem3[2] = circ(visible, cellheight, icon_x, listdistance3, COLOR_BROWN);
  listItem4[2] = circ(visible, cellheight, icon_x, listdistance4, COLOR_DARKBLUE);

  bar1 = rect(visible, barwidth, cellheight, bar1_x, bar_y, COLOR_ORANGE);
  bar2 = rect(visible, barwidth, cellheight, bar2_x, bar_y, COLOR_DARKGRAY);

  // small brackets
  constexpr uint8_t bracket_y = 183;
  constexpr uint8_t leftbracket_x = 110;
  constexpr uint8_t rightbracket_x = 220;
  constexpr uint8_t bracket_rect_w = 3;
  constexpr uint8_t bracket_rect_h = 22;
  constexpr uint8_t bracket_rect_y = bracket_y + 8;
  constexpr uint8_t leftbracket_rect_x = leftbracket_x - 1;
  constexpr uint8_t rightbracket_rect_x = rightbracket_x + 14;
  imgBracketLeft = lv_img_create(lv_scr_act(), nullptr);
  lv_obj_set_hidden(imgBracketLeft, !visible);
  lv_img_set_src(imgBracketLeft, &bracket_left);
  lv_obj_set_pos(imgBracketLeft, leftbracket_x, bracket_y);
  rectBracketLeft = rect(visible, bracket_rect_w, bracket_rect_h, leftbracket_rect_x, bracket_rect_y, COLOR_DARKBLUE);
  imgBracketRight = lv_img_create(lv_scr_act(), nullptr);
  lv_obj_set_hidden(imgBracketRight, !visible);
  lv_img_set_src(imgBracketRight, &bracket_right);
  lv_obj_set_pos(imgBracketRight, rightbracket_x, bracket_y);
  rectBracketRight = rect(visible, bracket_rect_w, bracket_rect_h, rightbracket_rect_x, bracket_rect_y, COLOR_DARKBLUE);

  batteryIcon.Create(lv_scr_act());
  batteryIcon.SetVisible(visible);
  batteryIcon.SetColor(COLOR_ICONS);
  lv_obj_align(batteryIcon.GetObject(), listItem1[1], LV_ALIGN_CENTER, -gap, 0);
  notificationIcon = label(visible, COLOR_ICONS, listItem2[1], LV_ALIGN_CENTER, -gap, 0, NotificationIcon::GetIcon(false));
  lv_img_set_angle(notificationIcon, 1500);
  bleIcon = label(visible, COLOR_ICONS, listItem3[1], LV_ALIGN_CENTER, -gap, 0, Symbols::bluetooth);
  batteryPlug = label(visible, COLOR_ICONS, listItem4[1], LV_ALIGN_CENTER, -gap, 0, Symbols::plug);

  label_dayname = label(visible, COLOR_DATE, listItem1[0], LV_ALIGN_IN_LEFT_MID, gap);
  label_day = label(visible, COLOR_DATE, listItem2[0], LV_ALIGN_IN_LEFT_MID, gap);
  label_month = label(visible, COLOR_DATE, listItem3[0], LV_ALIGN_IN_LEFT_MID, gap);
  label_year = label(visible, COLOR_DATE, listItem4[0], LV_ALIGN_IN_LEFT_MID, gap);

  hourAnchor = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(hourAnchor, "");
  minuteAnchor = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(minuteAnchor, "");
  setTimeAnchorForDisplaySeconds(settingsController.GetStarTrekDisplaySeconds());
  label_time_hour_1 = label(true, COLOR_TIME, hourAnchor, LV_ALIGN_OUT_RIGHT_MID, 2);
  lv_obj_set_style_local_text_font(label_time_hour_1, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_time);
  label_time_hour_10 = label(true, COLOR_TIME, hourAnchor, LV_ALIGN_OUT_LEFT_MID, -2);
  lv_obj_set_style_local_text_font(label_time_hour_10, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_time);
  label_time_min_1 = label(true, COLOR_TIME, minuteAnchor, LV_ALIGN_OUT_RIGHT_MID, 2);
  lv_obj_set_style_local_text_font(label_time_min_1, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_time);
  label_time_min_10 = label(true, COLOR_TIME, minuteAnchor, LV_ALIGN_OUT_LEFT_MID, -2);
  lv_obj_set_style_local_text_font(label_time_min_10, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_time);
  label_time_seconds = label(settingsController.GetStarTrekDisplaySeconds(), COLOR_TIME, minuteAnchor, LV_ALIGN_CENTER, 0, 46, "00");
  label_time_ampm = label(visible, COLOR_DATE, upperShape[2], LV_ALIGN_IN_BOTTOM_RIGHT, -30, -30);

  temperature = label(visible, COLOR_DATE, upperShape[1], LV_ALIGN_IN_BOTTOM_LEFT, gap, -2);
  weatherIcon = label(visible, COLOR_ICONS, temperature, LV_ALIGN_OUT_TOP_LEFT, 0, -gap);
  lv_obj_set_style_local_text_font(weatherIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &fontawesome_weathericons);
  heartbeatIcon = label(visible, COLOR_HEARTBEAT_OFF, lowerShape[3], LV_ALIGN_IN_TOP_LEFT, 5, gap, Symbols::heartBeat);
  heartbeatValue = label(visible, COLOR_ICONS, heartbeatIcon, LV_ALIGN_OUT_RIGHT_MID, 5);
  stepIcon = label(visible, COLOR_STEPS, imgBracketLeft, LV_ALIGN_OUT_RIGHT_MID, -2, 0, Symbols::shoe);
  stepValue = label(visible, COLOR_STEPS, stepIcon, LV_ALIGN_OUT_RIGHT_MID, 5, 0, "0");
  stepBar = lv_bar_create(lv_scr_act(), nullptr);
  lv_obj_set_hidden(stepBar, !visible);
  lv_obj_set_style_local_bg_color(stepBar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, COLOR_STEPS);
  lv_obj_set_size(stepBar, 92, 3);
  lv_obj_align(stepBar, imgBracketLeft, LV_ALIGN_OUT_RIGHT_BOTTOM, 1, 0);

  // menu buttons
  btnSetUseSystemFont = button(false, 228, 50, lv_scr_act(), LV_ALIGN_IN_TOP_MID, 0, 8);
  const char* label_sysfont =
    settingsController.GetStarTrekUseSystemFont() ? WANT_SYSTEM_FONT : (starTrekFontAvailable ? WANT_ST_FONT : WANT_ST_FONT_BUT_NO);
  lblSetUseSystemFont = label(true, COLOR_WHITE, btnSetUseSystemFont, LV_ALIGN_CENTER, 0, 0, label_sysfont, btnSetUseSystemFont);
  btnSetAnimate = button(false, 228, 50, btnSetUseSystemFont, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
  lblSetAnimate = label(true, COLOR_WHITE, btnSetAnimate, LV_ALIGN_CENTER, 0, 0, animateMenuButtonText(), btnSetAnimate);
  btnSetWeather = button(false, 228, 50, btnSetAnimate, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
  const char* label_weather = settingsController.GetStarTrekWeather() ? WANT_WEATHER : WANT_NO_WEATHER;
  lblSetWeather = label(true, COLOR_WHITE, btnSetWeather, LV_ALIGN_CENTER, 0, 0, label_weather, btnSetWeather);
  btnSetDisplaySeconds = button(false, 136, 50, lv_scr_act(), LV_ALIGN_IN_BOTTOM_LEFT, 8, -8);
  const char* label_displaySeconds = settingsController.GetStarTrekDisplaySeconds() ? WANT_SECONDS : WANT_MINUTES;
  lblSetDisplaySeconds = label(true, COLOR_WHITE, btnSetDisplaySeconds, LV_ALIGN_CENTER, 0, 0, label_displaySeconds, btnSetDisplaySeconds);
  btnClose = button(false, 80, 50, btnSetDisplaySeconds, LV_ALIGN_OUT_RIGHT_MID, 8);
  lblClose = label(true, COLOR_WHITE, btnClose, LV_ALIGN_CENTER, 0, 0, "X", btnClose);
}

void WatchFaceStarTrek::setTimeAnchorForDisplaySeconds(bool displaySeconds) {
  constexpr uint8_t TA_X = 175;
  constexpr uint8_t TA_NOSEC_H_Y = 47;
  constexpr uint8_t TA_NOSEC_M_Y = 122;
  constexpr int8_t TA_SEC_SHIFT_Y = -6;
  if (displaySeconds) {
    lv_obj_set_pos(hourAnchor, TA_X, TA_NOSEC_H_Y + TA_SEC_SHIFT_Y);
    lv_obj_set_pos(minuteAnchor, TA_X, TA_NOSEC_M_Y + TA_SEC_SHIFT_Y);
  } else {
    lv_obj_set_pos(hourAnchor, TA_X, TA_NOSEC_H_Y);
    lv_obj_set_pos(minuteAnchor, TA_X, TA_NOSEC_M_Y);
  }
}

void WatchFaceStarTrek::realignTime() {
  lv_obj_realign(label_time_hour_1);
  lv_obj_realign(label_time_hour_10);
  lv_obj_realign(label_time_min_1);
  lv_obj_realign(label_time_min_10);
  lv_obj_realign(label_time_seconds);
}

lv_obj_t* WatchFaceStarTrek::rect(bool visible, uint8_t w, uint8_t h, uint8_t x, uint8_t y, lv_color_t color) {
  lv_obj_t* rect = _base(visible, w, h, x, y, color);
  lv_obj_set_style_local_radius(rect, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 0);
  return rect;
}

lv_obj_t* WatchFaceStarTrek::circ(bool visible, uint8_t d, uint8_t x, uint8_t y, lv_color_t color) {
  lv_obj_t* circ = _base(visible, d, d, x, y, color);
  lv_obj_set_style_local_radius(circ, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
  return circ;
}

lv_obj_t* WatchFaceStarTrek::_base(bool visible, uint8_t w, uint8_t h, uint8_t x, uint8_t y, lv_color_t color) {
  lv_obj_t* base = lv_obj_create(lv_scr_act(), nullptr);
  lv_obj_set_hidden(base, !visible);
  lv_obj_set_style_local_bg_color(base, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
  lv_obj_set_size(base, w, h);
  lv_obj_set_pos(base, x, y);
  return base;
}

lv_obj_t* WatchFaceStarTrek::label(bool visible,
                                   lv_color_t color,
                                   lv_obj_t* alignto,
                                   lv_align_t alignmode,
                                   int16_t gapx,
                                   int16_t gapy,
                                   const char* text,
                                   lv_obj_t* base) {
  lv_obj_t* label = lv_label_create(base, nullptr);
  lv_obj_set_hidden(label, !visible);
  lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
  lv_label_set_text_static(label, text);
  lv_obj_align(label, alignto, alignmode, gapx, gapy);
  return label;
}

lv_obj_t* WatchFaceStarTrek::button(bool visible,
                                    uint16_t sizex,
                                    uint16_t sizey,
                                    lv_obj_t* alignto,
                                    lv_align_t alignmode,
                                    int16_t gapx,
                                    int16_t gapy) {
  lv_obj_t* btn = lv_btn_create(lv_scr_act(), nullptr);
  lv_obj_set_hidden(btn, !visible);
  btn->user_data = this;
  lv_obj_set_size(btn, sizex, sizey);
  lv_obj_align(btn, alignto, alignmode, gapx, gapy);
  lv_obj_set_style_local_bg_opa(btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_70);
  lv_obj_set_event_cb(btn, event_handler);
  lv_obj_set_hidden(btn, true);
  return btn;
}

// ########## Watch face content updates #######################################

void WatchFaceStarTrek::Refresh() {
  powerPresent = batteryController.IsPowerPresent();
  if (powerPresent.IsUpdated()) {
    lv_label_set_text_static(batteryPlug, BatteryIcon::GetPlugIcon(powerPresent.Get()));
  }

  batteryPercentRemaining = batteryController.PercentRemaining();
  if (batteryPercentRemaining.IsUpdated()) {
    auto batteryPercent = batteryPercentRemaining.Get();
    batteryIcon.SetBatteryPercentage(batteryPercent);
  }

  bleState = bleController.IsConnected();
  bleRadioEnabled = bleController.IsRadioEnabled();
  if (bleState.IsUpdated() || bleRadioEnabled.IsUpdated()) {
    lv_label_set_text_static(bleIcon, BleIcon::GetIcon(bleState.Get()));
  }

  notificationState = notificatioManager.AreNewNotificationsAvailable();
  if (notificationState.IsUpdated()) {
    lv_label_set_text_static(notificationIcon, NotificationIcon::GetIcon(notificationState.Get()));
  }
  lv_obj_realign(notificationIcon);

  currentDateTime = dateTimeController.CurrentDateTime();

  if (currentDateTime.IsUpdated()) {
    auto hour = dateTimeController.Hours();
    auto minute = dateTimeController.Minutes();
    auto second = dateTimeController.Seconds();
    auto year = dateTimeController.Year();
    auto month = dateTimeController.Month();
    auto dayOfWeek = dateTimeController.DayOfWeek();
    auto day = dateTimeController.Day();

    if (settingsController.GetStarTrekDisplaySeconds()) {
      if (displayedSecond != second) {
        lv_label_set_text_fmt(label_time_seconds, "%02d", second);
      }
    }

    if (displayedMinute != minute || displayedHour != hour) {
      displayedHour = hour;
      displayedMinute = minute;

      if (settingsController.GetClockType() == Controllers::Settings::ClockType::H12) {
        char ampmChar[3] = "AM";
        if (hour == 0) {
          hour = 12;
        } else if (hour == 12) {
          ampmChar[0] = 'P';
        } else if (hour > 12) {
          hour = hour - 12;
          ampmChar[0] = 'P';
        }
        lv_label_set_text(label_time_ampm, ampmChar);
      }
      lv_label_set_text_fmt(label_time_hour_1, "%d", hour % 10);
      lv_label_set_text_fmt(label_time_hour_10, "%d", hour / 10);
      lv_label_set_text_fmt(label_time_min_1, "%d", minute % 10);
      lv_label_set_text_fmt(label_time_min_10, "%d", minute / 10);
      realignTime();
    }

    if ((day != currentDay) || (dayOfWeek != currentDayOfWeek) || (month != currentMonth) || (year != currentYear)) {
      lv_label_set_text_fmt(label_dayname, "%s", dateTimeController.DayOfWeekShortToString());
      lv_label_set_text_fmt(label_day, "%02d", day);
      lv_label_set_text_fmt(label_month, "%s", dateTimeController.MonthShortToString());
      lv_label_set_text_fmt(label_year, "%d", year);
      currentYear = year;
      currentMonth = month;
      currentDayOfWeek = dayOfWeek;
      currentDay = day;
    }
  }

  heartbeat = heartRateController.HeartRate();
  heartbeatRunning = heartRateController.State() != Controllers::HeartRateController::States::Stopped;
  if (heartbeat.IsUpdated() || heartbeatRunning.IsUpdated()) {
    if (heartbeatRunning.Get()) {
      lv_obj_set_style_local_text_color(heartbeatIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_HEARTBEAT_ON);
      lv_label_set_text_fmt(heartbeatValue, "%d", heartbeat.Get());
    } else {
      lv_obj_set_style_local_text_color(heartbeatIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_HEARTBEAT_OFF);
      lv_label_set_text_static(heartbeatValue, "");
    }
    lv_obj_realign(heartbeatValue);
  }

  stepCount = motionController.NbSteps();
  if (stepCount.IsUpdated()) {
    lv_label_set_text_fmt(stepValue, "%lu", stepCount.Get());
    lv_obj_realign(stepValue);
    lv_bar_set_value(stepBar, stepCount.Get() * 100 / settingsController.GetStepsGoal(), LV_ANIM_OFF);
  }

  if (settingsController.GetStarTrekWeather()) {
    if (weatherService.GetCurrentTemperature()->timestamp != 0 && weatherService.GetCurrentClouds()->timestamp != 0 &&
        weatherService.GetCurrentPrecipitation()->timestamp != 0) {
      nowTemp = weatherService.GetCurrentTemperature()->temperature / 100;
      clouds = weatherService.GetCurrentClouds()->amount;
      precip = weatherService.GetCurrentPrecipitation()->amount;
      if (nowTemp.IsUpdated()) {
        lv_label_set_text_fmt(temperature, "%d°", nowTemp.Get());
        lv_obj_realign(temperature);
      }
      if (clouds.IsUpdated() || precip.IsUpdated()) {
        uint16_t c = clouds.Get();
        uint16_t p = clouds.Get();
        // this figuring out what icon to display could be done by weather service itself.
        // This code is in every watchface that supports weather ;)
        if ((c <= 30) && (p == 0)) {
          lv_label_set_text(weatherIcon, Symbols::sun);
        } else if ((c >= 70) && (c <= 90) && (p == 1)) {
          lv_label_set_text(weatherIcon, Symbols::cloudSunRain);
        } else if ((c > 90) && (p == 0)) {
          lv_label_set_text(weatherIcon, Symbols::cloud);
        } else if ((c > 70) && (p >= 2)) {
          lv_label_set_text(weatherIcon, Symbols::cloudShowersHeavy);
        } else {
          lv_label_set_text(weatherIcon, Symbols::cloudSun);
        };
        lv_obj_realign(weatherIcon);
      }
    } else {
      lv_label_set_text_static(temperature, "--");
      lv_label_set_text(weatherIcon, "");
      lv_obj_realign(temperature);
      lv_obj_realign(weatherIcon);
    }
  }

  if (!lv_obj_get_hidden(btnClose)) {
    if ((settingsAutoCloseTick > 0) && (lv_tick_get() - settingsAutoCloseTick > SETTINGS_AUTO_CLOSE_TICKS)) {
      setMenuButtonsVisible(false);
      settingsAutoCloseTick = 0;
    }
  }

  Settings::StarTrekAnimateType animateType = settingsController.GetStarTrekAnimate();
  if (animateType != Settings::StarTrekAnimateType::None) {
    if (animateType == Settings::StarTrekAnimateType::All || animateType == Settings::StarTrekAnimateType::Start) {
      if (!startAnimationFinished && lv_tick_get() - animatorStartTick > ANIMATOR_START_TICKS) {
        animateStartStep();
      }
    }
    if (animateType == Settings::StarTrekAnimateType::All || animateType == Settings::StarTrekAnimateType::Continuous) {
      if (lv_tick_get() - animatorContinuousTick > ANIMATOR_CONTINUOUS_TICKS) {
        animateContinuousStep();
      }
    }
  }
}

// ########## Style and animations #############################################

void WatchFaceStarTrek::updateFontTime() {
  lv_obj_set_style_local_text_font(label_time_hour_1, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_time);
  lv_obj_align(label_time_hour_1, hourAnchor, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
  lv_obj_set_style_local_text_font(label_time_hour_10, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_time);
  lv_obj_align(label_time_hour_10, hourAnchor, LV_ALIGN_OUT_LEFT_MID, -2, 0);
  lv_obj_set_style_local_text_font(label_time_min_1, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_time);
  lv_obj_align(label_time_min_1, minuteAnchor, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
  lv_obj_set_style_local_text_font(label_time_min_10, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_time);
  lv_obj_align(label_time_min_10, minuteAnchor, LV_ALIGN_OUT_LEFT_MID, -2, 0);
}

void WatchFaceStarTrek::animateStartStep() {
  switch (animatorStartStage) {
    case 0:
      lv_obj_set_hidden(topRightRect, false);
      setShapeVisible(upperShape, PART_COUNT_UPPER_SHAPE, true);
      lv_obj_set_hidden(weatherIcon, false);
      lv_obj_set_hidden(temperature, false);
      lv_obj_set_hidden(label_time_ampm, false);
      break;
    case 1:
      setShapeVisible(listItem1, PART_COUNT_LIST_ITEM, true);
      lv_obj_set_hidden(label_dayname, false);
      batteryIcon.SetVisible(true);
      break;
    case 2:
      setShapeVisible(listItem2, PART_COUNT_LIST_ITEM, true);
      lv_obj_set_hidden(label_day, false);
      lv_obj_set_hidden(notificationIcon, false);
      break;
    case 3:
      setShapeVisible(listItem3, PART_COUNT_LIST_ITEM, true);
      lv_obj_set_hidden(label_month, false);
      lv_obj_set_hidden(bleIcon, false);
      break;
    case 4:
      setShapeVisible(listItem4, PART_COUNT_LIST_ITEM, true);
      lv_obj_set_hidden(label_year, false);
      lv_obj_set_hidden(batteryPlug, false);
      break;
    case 5:
      setShapeVisible(lowerShape, PART_COUNT_LOWER_SHAPE, true);
      lv_obj_set_hidden(bottomRightRect, false);
      break;
    case 6:
      lv_obj_set_hidden(bar1, false);
      lv_obj_set_hidden(bar2, false);
      lv_obj_set_hidden(imgBracketLeft, false);
      lv_obj_set_hidden(rectBracketLeft, false);
      lv_obj_set_hidden(imgBracketRight, false);
      lv_obj_set_hidden(rectBracketRight, false);
      lv_obj_set_hidden(heartbeatIcon, false);
      lv_obj_set_hidden(heartbeatValue, false);
      lv_obj_set_hidden(stepIcon, false);
      lv_obj_set_hidden(stepValue, false);
      lv_obj_set_hidden(stepBar, false);
      startAnimationFinished = true;
      break;
  }
  animatorStartStage++;
  animatorStartTick = lv_tick_get();
}

void WatchFaceStarTrek::animateContinuousStep() {
  // if Start animation is also active, only act if it is already finished
  if (settingsController.GetStarTrekAnimate() == Settings::StarTrekAnimateType::All && !startAnimationFinished)
    return;

  // flash the brackets
  bool hidden = animatorContinuousStage == 7;
  if (animatorContinuousStage > 6 && animatorContinuousStage < 9) {
    lv_obj_set_hidden(imgBracketLeft, hidden);
    lv_obj_set_hidden(imgBracketRight, hidden);
    lv_obj_set_hidden(stepBar, hidden);
  }

  // walk down list with color change, change some panel colors
  switch (animatorContinuousStage) {
    case 0:
      lv_obj_set_style_local_text_color(label_dayname, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_LIGHTGRAY);
      break;
    case 1:
      lv_obj_set_hidden(bar1, true);
      break;
    case 2:
      lv_obj_set_style_local_text_color(label_dayname, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DATE);
      lv_obj_set_style_local_text_color(label_day, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_LIGHTGRAY);
      lv_obj_set_hidden(bar2, true);
      break;
    case 3:
      lv_obj_set_hidden(bar1, false);
      lv_obj_set_style_local_bg_color(listItem3[0], LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DARKBLUE);
      break;
    case 4:
      lv_obj_set_style_local_text_color(label_day, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DATE);
      lv_obj_set_style_local_text_color(label_month, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_LIGHTGRAY);
      lv_obj_set_hidden(bar2, false);
      break;
    case 5:
      lv_obj_set_style_local_bg_color(topRightRect, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_BROWN);
      break;
    case 6:
      lv_obj_set_style_local_text_color(label_month, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DATE);
      lv_obj_set_style_local_text_color(label_year, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_LIGHTGRAY);
      lv_obj_set_hidden(rectBracketLeft, true);
      lv_obj_set_hidden(rectBracketRight, true);
      break;
    case 7:
      lv_obj_set_style_local_bg_color(listItem3[0], LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_LIGHTBLUE);
      break;
    case 8:
      lv_obj_set_style_local_text_color(label_year, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DATE);
      break;
    case 9:
      lv_obj_set_hidden(rectBracketLeft, false);
      lv_obj_set_hidden(rectBracketRight, false);
      break;
    case 10:
      lv_obj_set_style_local_bg_color(topRightRect, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DARKGRAY);
      break;
    case 11:
      animatorContinuousStage = 255; // overflows to 0 below
      break;
  }
  animatorContinuousStage++;
  animatorContinuousTick = lv_tick_get();
}

void WatchFaceStarTrek::resetColors() {
  lv_obj_set_style_local_text_color(label_dayname, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DATE);
  lv_obj_set_style_local_text_color(label_day, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DATE);
  lv_obj_set_style_local_text_color(label_month, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DATE);
  lv_obj_set_style_local_text_color(label_year, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DATE);
  lv_obj_set_style_local_bg_color(listItem3[0], LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_LIGHTBLUE);
  lv_obj_set_style_local_bg_color(topRightRect, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DARKGRAY);
}

void WatchFaceStarTrek::setShapeVisible(lv_obj_t** shape, uint8_t partcount, bool visible) {
  for (uint8_t i = 0; i < partcount; i++) {
    lv_obj_set_hidden(shape[i], !visible);
  }
}

void WatchFaceStarTrek::setVisible(bool visible) {
  lv_obj_set_hidden(topRightRect, !visible);
  setShapeVisible(upperShape, PART_COUNT_UPPER_SHAPE, visible);
  lv_obj_set_hidden(label_time_ampm, !visible);
  setShapeVisible(listItem1, PART_COUNT_LIST_ITEM, visible);
  lv_obj_set_hidden(label_dayname, !visible);
  batteryIcon.SetVisible(visible);
  setShapeVisible(listItem2, PART_COUNT_LIST_ITEM, visible);
  lv_obj_set_hidden(label_day, !visible);
  lv_obj_set_hidden(notificationIcon, !visible);
  setShapeVisible(listItem3, PART_COUNT_LIST_ITEM, visible);
  lv_obj_set_hidden(label_month, !visible);
  lv_obj_set_hidden(bleIcon, !visible);
  setShapeVisible(listItem4, PART_COUNT_LIST_ITEM, visible);
  lv_obj_set_hidden(label_year, !visible);
  lv_obj_set_hidden(batteryPlug, !visible);
  setShapeVisible(lowerShape, PART_COUNT_LOWER_SHAPE, visible);
  lv_obj_set_hidden(bottomRightRect, !visible);
  lv_obj_set_hidden(bar1, !visible);
  lv_obj_set_hidden(bar2, !visible);
  lv_obj_set_hidden(heartbeatIcon, !visible);
  lv_obj_set_hidden(heartbeatValue, !visible);
  lv_obj_set_hidden(stepIcon, !visible);
  lv_obj_set_hidden(stepValue, !visible);
  lv_obj_set_hidden(stepBar, !visible);
  lv_obj_set_hidden(weatherIcon, !visible);
  lv_obj_set_hidden(temperature, !visible);
  lv_obj_set_hidden(imgBracketLeft, !visible);
  lv_obj_set_hidden(rectBracketLeft, !visible);
  lv_obj_set_hidden(imgBracketRight, !visible);
  lv_obj_set_hidden(rectBracketRight, !visible);
}

void WatchFaceStarTrek::setMenuButtonsVisible(bool visible) {
  lv_obj_set_hidden(btnSetUseSystemFont, !visible);
  lv_obj_set_hidden(btnSetAnimate, !visible);
  lv_obj_set_hidden(btnSetDisplaySeconds, !visible);
  lv_obj_set_hidden(btnSetWeather, !visible);
  lv_obj_set_hidden(btnClose, !visible);
}

void WatchFaceStarTrek::startStartAnimation() {
  setVisible(false);
  startAnimationFinished = false;
  animatorStartTick = lv_tick_get();
  animatorStartStage = 0;
}

void WatchFaceStarTrek::startContinuousAnimation() {
  resetColors();
  animatorContinuousTick = lv_tick_get();
  animatorContinuousStage = 0;
}

// ########## Parent overrrides ################################################

bool WatchFaceStarTrek::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
  if ((event == Pinetime::Applications::TouchEvents::LongTap) && lv_obj_get_hidden(btnClose)) {
    setMenuButtonsVisible(true);
    settingsAutoCloseTick = lv_tick_get();
    return true;
  }
  if ((event == Pinetime::Applications::TouchEvents::DoubleTap) && (lv_obj_get_hidden(btnClose) == false)) {
    return true;
  }
  return false;
}

bool WatchFaceStarTrek::OnButtonPushed() {
  if (!lv_obj_get_hidden(btnClose)) {
    setMenuButtonsVisible(false);
    return true;
  }
  return false;
}

void WatchFaceStarTrek::OnLCDWakeup() {
  Settings::StarTrekAnimateType animateType = settingsController.GetStarTrekAnimate();
  if (animateType == Settings::StarTrekAnimateType::All || animateType == Settings::StarTrekAnimateType::Start) {
    startStartAnimation();
  }
  if (animateType == Settings::StarTrekAnimateType::All || animateType == Settings::StarTrekAnimateType::Continuous) {
    startContinuousAnimation();
  }
}

void WatchFaceStarTrek::OnLCDSleep() {
  Settings::StarTrekAnimateType animateType = settingsController.GetStarTrekAnimate();
  if (animateType == Settings::StarTrekAnimateType::All || animateType == Settings::StarTrekAnimateType::Start) {
    setVisible(false);
  }
  if (animateType == Settings::StarTrekAnimateType::All || animateType == Settings::StarTrekAnimateType::Continuous) {
    resetColors();
  }
}

// ########## Config handling ##################################################

void WatchFaceStarTrek::UpdateSelected(lv_obj_t* object, lv_event_t event) {
  if (event == LV_EVENT_CLICKED) {

    settingsAutoCloseTick = lv_tick_get();

    if (object == btnClose) {
      setMenuButtonsVisible(false);
    }

    if (object == btnSetUseSystemFont) {
      bool usedSystem = settingsController.GetStarTrekUseSystemFont();
      // ST font was not used and shall be used now
      if (starTrekFontAvailable && usedSystem) {
        lv_label_set_text_static(lblSetUseSystemFont, WANT_ST_FONT);
        font_time = lv_font_load("F:/fonts/edge_of_the_galaxy.bin");
        // give font time to be loaded, this can possibly be lowered
        vTaskDelay(100);
        updateFontTime();
        usedSystem = false;
      }
      // ST font was used and gets deactivated
      else if (starTrekFontAvailable && !usedSystem) {
        lv_label_set_text_static(lblSetUseSystemFont, WANT_SYSTEM_FONT);
        if (font_time != nullptr) {
          lv_font_free(font_time);
        }
        font_time = &jetbrains_mono_extrabold_compressed;
        updateFontTime();
        usedSystem = true;
      }
      // ST font was not used, shall be used now, but is not available
      // ST font should be used by default but is not installed
      else {
        lv_label_set_text_static(lblSetUseSystemFont, WANT_ST_FONT_BUT_NO);
      }
      settingsController.SetStarTrekUseSystemFont(usedSystem);
    }

    if (object == btnSetAnimate) {
      Settings::StarTrekAnimateType next = animateStateCycler(settingsController.GetStarTrekAnimate());
      if (next == Settings::StarTrekAnimateType::All || next == Settings::StarTrekAnimateType::Start) {
        startStartAnimation();
      } else {
        setVisible();
      }
      resetColors();
      settingsController.SetStarTrekAnimate(next);
      lv_label_set_text_static(lblSetAnimate, animateMenuButtonText());
    }

    if (object == btnSetDisplaySeconds) {
      if (settingsController.GetStarTrekDisplaySeconds()) {
        settingsController.SetStarTrekDisplaySeconds(false);
        setTimeAnchorForDisplaySeconds(false);
        lv_obj_set_hidden(label_time_seconds, true);
      } else {
        settingsController.SetStarTrekDisplaySeconds(true);
        setTimeAnchorForDisplaySeconds(true);
        lv_obj_set_hidden(label_time_seconds, false);
      }
      realignTime();
      lv_label_set_text_static(lblSetDisplaySeconds, settingsController.GetStarTrekDisplaySeconds() ? WANT_SECONDS : WANT_MINUTES);
    }

    if (object == btnSetWeather) {
      if (settingsController.GetStarTrekWeather()) {
        settingsController.SetStarTrekWeather(false);
        lv_label_set_text_static(weatherIcon, "");
        lv_label_set_text_static(temperature, "");
        lv_label_set_text_static(lblSetWeather, WANT_NO_WEATHER);
      } else {
        settingsController.SetStarTrekWeather(true);
        // icon and temp will be set by next refresh
        lv_label_set_text_static(lblSetWeather, WANT_WEATHER);
      }
    }
  }
}

const char* WatchFaceStarTrek::animateMenuButtonText() {
  switch (settingsController.GetStarTrekAnimate()) {
    case Settings::StarTrekAnimateType::All:
      return WANT_ANIMATE_ALL;
    case Settings::StarTrekAnimateType::Continuous:
      return WANT_ANIMATE_CONTINUOUS;
    case Settings::StarTrekAnimateType::Start:
      return WANT_ANIMATE_START;
    case Settings::StarTrekAnimateType::None:
      return WANT_STATIC;
  }
  return "Error";
}

Settings::StarTrekAnimateType WatchFaceStarTrek::animateStateCycler(Settings::StarTrekAnimateType previous) {
  switch (previous) {
    case Settings::StarTrekAnimateType::All:
      return Settings::StarTrekAnimateType::Start;
    case Settings::StarTrekAnimateType::Start:
      return Settings::StarTrekAnimateType::Continuous;
    case Settings::StarTrekAnimateType::Continuous:
      return Settings::StarTrekAnimateType::None;
    case Settings::StarTrekAnimateType::None:
      return Settings::StarTrekAnimateType::All;
  }
  return Settings::StarTrekAnimateType::None;
}