#include "displayapp/screens/WatchFaceStarTrek.h"

#include <date/date.h>
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
#include "components/settings/Settings.h"
using namespace Pinetime::Applications::Screens;

#define COLOR_TIME          lv_color_hex(0x83ddff) // LIGHTBLUE
#define COLOR_DATE          lv_color_hex(0x222222)
#define COLOR_ICONS         lv_color_hex(0x111111)
#define COLOR_HEARTBEAT_ON  lv_color_hex(0xff4f10)
#define COLOR_HEARTBEAT_OFF lv_color_hex(0x3786ff) // BLUE
#define COLOR_STEPS         lv_color_hex(0xada88b) // BEIGE
#define COLOR_BG            lv_color_hex(0x000000) // BLACK

#define COLOR_LIGHTBLUE lv_color_hex(0x83ddff)
#define COLOR_DARKBLUE  lv_color_hex(0x0946ee)
#define COLOR_BLUE      lv_color_hex(0x3786ff)
#define COLOR_ORANGE    lv_color_hex(0xd45f10)
#define COLOR_DARKGRAY  lv_color_hex(0x48606c)
#define COLOR_BEIGE     lv_color_hex(0xada88b)
#define COLOR_BROWN     lv_color_hex(0x644400)
#define COLOR_BLACK     lv_color_hex(0x000000)
#define COLOR_WHITE     lv_color_hex(0xffffff)

WatchFaceStarTrek::WatchFaceStarTrek(DisplayApp* app,
                                     Controllers::DateTime& dateTimeController,
                                     Controllers::Battery& batteryController,
                                     Controllers::Ble& bleController,
                                     Controllers::NotificationManager& notificatioManager,
                                     Controllers::Settings& settingsController,
                                     Controllers::HeartRateController& heartRateController,
                                     Controllers::MotionController& motionController,
                                     Controllers::FS& filesystem)
  : Screen(app),
    currentDateTime {{}},
    dateTimeController {dateTimeController},
    batteryController {batteryController},
    bleController {bleController},
    notificatioManager {notificatioManager},
    settingsController {settingsController},
    heartRateController {heartRateController},
    motionController {motionController} {

  lfs_file f = {};
  if (filesystem.FileOpen(&f, "/fonts/edge_of_the_galaxy.bin", LFS_O_RDONLY) >= 0) {
    filesystem.FileClose(&f);
    font_time = lv_font_load("F:/fonts/edge_of_the_galaxy.bin");
  } else {
    font_time = &jetbrains_mono_extrabold_compressed;
  }

  // draw background
  // grid organizing stuff
  const uint8_t gap = 3, cellheight = 26, upperend = 80;
  const uint8_t cpg = cellheight + gap;
  const uint8_t upg = upperend + gap;
  const uint8_t listend = upperend + 4 * cellheight + 5 * gap;
  // upper
  topRightRect = rect(84, 11, 156, 0, COLOR_DARKGRAY);
  upperShapeRect1 = rect(85, 11, 68, 0, COLOR_BLUE);
  upperShapeRect2 = rect(72, 46, 34, 34, COLOR_BLUE);
  upperShapeRect3 = rect(36, 80, 70, 0, COLOR_BLUE);
  upperShapeRect4 = rect(14, 14, 106, 11, COLOR_BLUE);
  upperShapeCirc1 = circ(68, 34, 0, COLOR_BLUE);
  upperShapeCirc2 = circ(28, 106, 11, COLOR_BG);
  // lower
  lowerShapeCirc1 = circ(68, 34, 172, COLOR_BLUE);         // draw these two first, because circle is to big
  lowerShapeCircHalfCut = rect(68, 34, 34, 172, COLOR_BG); // and has to be occluded by this
  bottomRightRect = rect(84, 11, 156, 229, COLOR_ORANGE);
  lowerShapeRect1 = rect(85, 11, 68, 229, COLOR_BLUE);
  lowerShapeRect2 = rect(72, 240 - listend - 34, 34, listend, COLOR_BLUE);
  lowerShapeRect3 = rect(36, 240 - listend, 70, listend, COLOR_BLUE);
  lowerShapeRect4 = rect(14, 14, 106, 215, COLOR_BLUE);
  lowerShapeCirc2 = circ(28, 106, 201, COLOR_BG);
  // date list
  dateRect1 = rect(72, cellheight, 34, upg, COLOR_BEIGE);
  dateRect2 = rect(72, cellheight, 34, upg + cpg, COLOR_ORANGE);
  dateRect3 = rect(72, cellheight, 34, upg + 2 * cpg, COLOR_LIGHTBLUE);
  dateRect4 = rect(72, cellheight, 34, upg + 3 * cpg, COLOR_DARKGRAY);
  // icon list
  iconRect1 = rect(17, cellheight, 14, upg, COLOR_LIGHTBLUE);
  iconRect2 = rect(17, cellheight, 14, upg + cpg, COLOR_BEIGE);
  iconRect3 = rect(17, cellheight, 14, upg + 2 * cpg, COLOR_BROWN);
  iconRect4 = rect(17, cellheight, 14, upg + 3 * cpg, COLOR_DARKBLUE);
  iconCirc1 = circ(cellheight, 0, upg, COLOR_LIGHTBLUE);
  iconCirc2 = circ(cellheight, 0, upg + cpg, COLOR_BEIGE);
  iconCirc3 = circ(cellheight, 0, upg + 2 * cpg, COLOR_BROWN);
  iconCirc4 = circ(cellheight, 0, upg + 3 * cpg, COLOR_DARKBLUE);
  // bars
  bar1 = rect(4, cellheight, 27, upperend - cellheight, COLOR_ORANGE);
  bar1 = rect(4, cellheight, 20, upperend - cellheight, COLOR_DARKGRAY);
  // brackets
  img_bracketLeft = lv_img_create(lv_scr_act(), NULL);
  lv_img_set_src(img_bracketLeft, "F:/images/startrek_bracket_left.bin");
  lv_obj_set_pos(img_bracketLeft, 109, 180);
  img_bracketRight = lv_img_create(lv_scr_act(), NULL);
  lv_img_set_src(img_bracketRight, "F:/images/startrek_bracket_right.bin");
  lv_obj_set_pos(img_bracketRight, 216, 180);

  // put info on background
  batteryIcon.Create(lv_scr_act());
  batteryIcon.SetColor(COLOR_ICONS);
  lv_obj_align(batteryIcon.GetObject(), iconRect1, LV_ALIGN_CENTER, -3, 0);

  notificationIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(notificationIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_ICONS);
  lv_label_set_text_static(notificationIcon, NotificationIcon::GetIcon(false));
  lv_obj_align(notificationIcon, iconRect2, LV_ALIGN_CENTER, -3, 0);
  lv_img_set_angle(notificationIcon, 1500);

  bleIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(bleIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_ICONS);
  lv_label_set_text_static(bleIcon, Symbols::bluetooth);
  lv_obj_align(bleIcon, iconRect3, LV_ALIGN_CENTER, -3, 0);

  batteryPlug = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(batteryPlug, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_ICONS);
  lv_label_set_text_static(batteryPlug, Symbols::plug);
  lv_obj_align(batteryPlug, iconRect4, LV_ALIGN_CENTER, -3, 0);

  label_dayname = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(label_dayname, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DATE);
  lv_obj_align(label_dayname, dateRect1, LV_ALIGN_IN_LEFT_MID, gap, 0);

  label_day = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(label_day, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DATE);
  lv_obj_align(label_day, dateRect2, LV_ALIGN_IN_LEFT_MID, gap, 0);

  label_month = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(label_month, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DATE);
  lv_obj_align(label_month, dateRect3, LV_ALIGN_IN_LEFT_MID, gap, 0);

  label_year = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(label_year, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DATE);
  lv_obj_align(label_year, dateRect4, LV_ALIGN_IN_LEFT_MID, gap, 0);

  hourAnchor = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(hourAnchor, "");
  lv_obj_set_pos(hourAnchor, 175, 47);
  label_time_hour_1 = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_time_hour_1, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_time);
  lv_obj_set_style_local_text_color(label_time_hour_1, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_TIME);
  lv_obj_align(label_time_hour_1, hourAnchor, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
  label_time_hour_10 = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_time_hour_10, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_time);
  lv_obj_set_style_local_text_color(label_time_hour_10, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_TIME);
  lv_obj_align(label_time_hour_10, hourAnchor, LV_ALIGN_OUT_LEFT_MID, -2, 0);

  minuteAnchor = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(minuteAnchor, "");
  lv_obj_set_pos(minuteAnchor, 175, 122);
  label_time_min_1 = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_time_min_1, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_time);
  lv_obj_set_style_local_text_color(label_time_min_1, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_TIME);
  lv_obj_align(label_time_min_1, minuteAnchor, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
  label_time_min_10 = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_time_min_10, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_time);
  lv_obj_set_style_local_text_color(label_time_min_10, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_TIME);
  lv_obj_align(label_time_min_10, minuteAnchor, LV_ALIGN_OUT_LEFT_MID, -2, 0);

  label_time_ampm = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(label_time_ampm, "");
  lv_obj_align(label_time_ampm, upperShapeRect3, LV_ALIGN_IN_BOTTOM_RIGHT, -30, -30);
  lv_obj_set_style_local_text_color(label_time_ampm, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_DATE);

  heartbeatIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(heartbeatIcon, Symbols::heartBeat);
  lv_obj_set_style_local_text_color(heartbeatIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_HEARTBEAT_OFF);
  lv_obj_align(heartbeatIcon, upperShapeRect2, LV_ALIGN_IN_BOTTOM_LEFT, gap, 0);

  heartbeatValue = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(heartbeatValue, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_ICONS);
  lv_label_set_text_static(heartbeatValue, "");
  lv_obj_align(heartbeatValue, heartbeatIcon, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  stepIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(stepIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_STEPS);
  lv_label_set_text_static(stepIcon, Symbols::shoe);
  lv_obj_align(stepIcon, img_bracketLeft, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

  stepValue = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(stepValue, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_STEPS);
  lv_label_set_text_static(stepValue, "0");
  lv_obj_align(stepValue, stepIcon, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  Refresh();
}

WatchFaceStarTrek::~WatchFaceStarTrek() {
  lv_task_del(taskRefresh);

  if (font_time != nullptr) {
    lv_font_free(font_time);
  }

  lv_obj_clean(lv_scr_act());
}

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
    auto newDateTime = currentDateTime.Get();

    auto dp = date::floor<date::days>(newDateTime);
    auto time = date::make_time(newDateTime - dp);
    auto yearMonthDay = date::year_month_day(dp);

    auto year = static_cast<int>(yearMonthDay.year());
    auto month = static_cast<Pinetime::Controllers::DateTime::Months>(static_cast<unsigned>(yearMonthDay.month()));
    auto day = static_cast<unsigned>(yearMonthDay.day());
    auto dayOfWeek = static_cast<Pinetime::Controllers::DateTime::Days>(date::weekday(yearMonthDay).iso_encoding());

    uint8_t hour = time.hours().count();
    uint8_t minute = time.minutes().count();

    if (displayedHour != hour || displayedMinute != minute) {
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
        lv_label_set_text_fmt(label_time_hour_1, "%d", hour % 10);
        lv_label_set_text_fmt(label_time_hour_10, "%d", hour / 10);
        lv_label_set_text_fmt(label_time_min_1, "%d", minute % 10);
        lv_label_set_text_fmt(label_time_min_10, "%d", minute / 10);
        lv_obj_realign(label_time_hour_1);
        lv_obj_realign(label_time_hour_10);
        lv_obj_realign(label_time_min_1);
        lv_obj_realign(label_time_min_10);
      } else {
        lv_label_set_text_fmt(label_time_hour_1, "%d", hour % 10);
        lv_label_set_text_fmt(label_time_hour_10, "%d", hour / 10);
        lv_label_set_text_fmt(label_time_min_1, "%d", minute % 10);
        lv_label_set_text_fmt(label_time_min_10, "%d", minute / 10);
        lv_obj_realign(label_time_hour_1);
        lv_obj_realign(label_time_hour_10);
        lv_obj_realign(label_time_min_1);
        lv_obj_realign(label_time_min_10);
      }
    }

    if ((year != currentYear) || (month != currentMonth) || (dayOfWeek != currentDayOfWeek) || (day != currentDay)) {
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
  motionSensorOk = motionController.IsSensorOk();
  if (stepCount.IsUpdated() || motionSensorOk.IsUpdated()) {
    lv_label_set_text_fmt(stepValue, "%lu", stepCount.Get());
    lv_obj_realign(stepValue);
  }
}

lv_obj_t* WatchFaceStarTrek::rect(uint8_t w, uint8_t h, uint8_t x, uint8_t y, lv_color_t color) {
  lv_obj_t* rect = _base(w, h, x, y, color);
  lv_obj_set_style_local_radius(rect, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 0);
  return rect;
}

lv_obj_t* WatchFaceStarTrek::circ(uint8_t d, uint8_t x, uint8_t y, lv_color_t color) {
  lv_obj_t* circ = _base(d, d, x, y, color);
  lv_obj_set_style_local_radius(circ, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
  return circ;
}

lv_obj_t* WatchFaceStarTrek::_base(uint8_t w, uint8_t h, uint8_t x, uint8_t y, lv_color_t color) {
  lv_obj_t* base = lv_obj_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_bg_color(base, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
  lv_obj_set_size(base, w, h);
  lv_obj_set_pos(base, x, y);
  return base;
}

bool WatchFaceStarTrek::IsAvailable(Pinetime::Controllers::FS& filesystem) {
  lfs_file file = {};

  if (filesystem.FileOpen(&file, "/fonts/edge_of_the_galaxy.bin", LFS_O_RDONLY) < 0) {
    return false;
  }
  filesystem.FileClose(&file);
  if (filesystem.FileOpen(&file, "/images/startrek_bracket_right.bin", LFS_O_RDONLY) < 0) {
    return false;
  }
  filesystem.FileClose(&file);
  if (filesystem.FileOpen(&file, "/images/startrek_bracket_left.bin", LFS_O_RDONLY) < 0) {
    return false;
  }
  filesystem.FileClose(&file);

  return true;
}