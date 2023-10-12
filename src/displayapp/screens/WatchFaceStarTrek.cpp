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
#include "components/settings/Settings.h"
using namespace Pinetime::Applications::Screens;

constexpr lv_color_t COLOR_LIGHTBLUE = LV_COLOR_MAKE(0x83, 0xdd, 0xff);
constexpr lv_color_t COLOR_DARKBLUE = LV_COLOR_MAKE(0x09, 0x46, 0xee);
constexpr lv_color_t COLOR_BLUE = LV_COLOR_MAKE(0x37, 0x86, 0xff);
constexpr lv_color_t COLOR_ORANGE = LV_COLOR_MAKE(0xd4, 0x5f, 0x10);
constexpr lv_color_t COLOR_DARKGRAY = LV_COLOR_MAKE(0x48, 0x60, 0x6c);
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

// watch out for sizes !
constexpr char WANT_SYSTEM_FONT[12] = "System font";
constexpr char WANT_ST_FONT[15] = "Star Trek font";
constexpr char WANT_ST_FONT_BUT_NO[14] = "Not installed";
constexpr char WANT_ANIMATE[8] = "Animate";
constexpr char WANT_STATIC[7] = "Static";

constexpr uint16_t SETTINGS_AUTO_CLOSE_TICKS = 5000;
constexpr uint16_t ANIMATOR_START_TICKS = 50;

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
                                     Controllers::FS& filesystem)
  : currentDateTime {{}},
    batteryIcon(false),
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

  drawWatchFace();

  if (settingsController.GetStarTrekAnimate()) {
    startAnimation();
  }

  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  Refresh();
}

void WatchFaceStarTrek::drawWatchFace() {
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
  dateRect1 = rect(cellwidth, cellheight, cells_x, upg, COLOR_BEIGE);
  dateRect2 = rect(cellwidth, cellheight, cells_x, listdistance2, COLOR_ORANGE);
  dateRect3 = rect(cellwidth, cellheight, cells_x, listdistance3, COLOR_LIGHTBLUE);
  dateRect4 = rect(cellwidth, cellheight, cells_x, listdistance4, COLOR_DARKGRAY);
  // icon list
  iconRect1 = rect(iconrectwidth, cellheight, iconrect_x, upg, COLOR_LIGHTBLUE);
  iconRect2 = rect(iconrectwidth, cellheight, iconrect_x, listdistance2, COLOR_BEIGE);
  iconRect3 = rect(iconrectwidth, cellheight, iconrect_x, listdistance3, COLOR_BROWN);
  iconRect4 = rect(iconrectwidth, cellheight, iconrect_x, listdistance4, COLOR_DARKBLUE);
  iconCirc1 = circ(cellheight, icon_x, upg, COLOR_LIGHTBLUE);
  iconCirc2 = circ(cellheight, icon_x, listdistance2, COLOR_BEIGE);
  iconCirc3 = circ(cellheight, icon_x, listdistance3, COLOR_BROWN);
  iconCirc4 = circ(cellheight, icon_x, listdistance4, COLOR_DARKBLUE);
  // bars
  bar1 = rect(barwidth, cellheight, bar1_x, bar_y, COLOR_ORANGE);
  bar2 = rect(barwidth, cellheight, bar2_x, bar_y, COLOR_DARKGRAY);

  // small brackets
  // -- global location
  constexpr uint8_t bracket_y = 182;
  constexpr uint8_t leftbracket_x = 109;
  constexpr uint8_t rightbracket_x = 237;
  constexpr uint8_t bgap = 1;
  // -- dimensions of shapes
  constexpr uint8_t bracketrect1_w = 9;
  constexpr uint8_t bracketrect1_h = 3;
  constexpr uint8_t bracketrect2_h = 9;
  constexpr uint8_t vertical_w = 4;
  constexpr uint8_t smallrect_h = 6;
  constexpr uint8_t largerect_h = 22;
  constexpr uint8_t stickout_w = 3;
  constexpr uint8_t stickout_h = 4;
  constexpr uint8_t bcirc_d = 6;
  constexpr uint8_t scirc_d = 4;
  constexpr uint8_t bcircshift = 3;
  constexpr uint8_t scircshift = 4;
  // -- positions
  constexpr uint8_t vertical_x = leftbracket_x + vertical_w + bgap;
  constexpr uint8_t vertical2_x = vertical_x + vertical_w;
  constexpr uint8_t stickout_y = bracket_y + bracketrect1_h + bgap;
  constexpr uint8_t largerect_y = stickout_y + stickout_h + bgap;
  constexpr uint8_t smallrect1_y = bracket_y + bracketrect1_h + bracketrect2_h + bgap;
  constexpr uint8_t smallrect2_y = smallrect1_y + smallrect_h + bgap;
  constexpr uint8_t bottom1_y = smallrect2_y + smallrect_h + bgap;
  constexpr uint8_t stickout2_y = largerect_y + largerect_h + bgap;
  constexpr uint8_t bottom2_y = bottom1_y + bracketrect2_h;
  constexpr uint8_t rightvertical_x = rightbracket_x - 2 * vertical_w - bgap;

  bracket1[0] = rect(vertical_w, largerect_h, leftbracket_x, largerect_y, COLOR_DARKBLUE);
  bracket1[1] = rect(bracketrect1_w, bracketrect1_h, vertical2_x, bracket_y, COLOR_LIGHTBLUE);
  bracket1[2] = circ(bcirc_d, vertical_x, bracket_y, COLOR_LIGHTBLUE);
  bracket1[3] = rect(vertical_w, bracketrect2_h, vertical_x, bracket_y + bracketrect1_h, COLOR_LIGHTBLUE);
  bracket1[4] = circ(scirc_d, vertical2_x, bracket_y + bracketrect1_h, COLOR_BLACK);
  bracket1[5] = rect(stickout_w, stickout_h, vertical_x - stickout_w, stickout_y, COLOR_LIGHTBLUE);
  bracket1[6] = rect(vertical_w, smallrect_h, vertical_x, smallrect1_y, COLOR_DARKGRAY);
  bracket1[7] = rect(vertical_w, smallrect_h, vertical_x, smallrect2_y, COLOR_ORANGE);
  bracket1[8] = rect(vertical_w, bracketrect2_h, vertical_x, bottom1_y, COLOR_LIGHTBLUE);
  bracket1[9] = rect(stickout_w, stickout_h, vertical_x - stickout_w, stickout2_y, COLOR_LIGHTBLUE);
  bracket1[10] = circ(bcirc_d, vertical_x, bottom2_y - bcircshift, COLOR_LIGHTBLUE);
  bracket1[11] = rect(bracketrect1_w, bracketrect1_h, vertical2_x, bottom2_y, COLOR_LIGHTBLUE);
  bracket1[12] = circ(scirc_d, vertical2_x, bottom2_y - scircshift, COLOR_BLACK);

  bracket2[0] = rect(vertical_w, largerect_h, rightbracket_x - vertical_w, largerect_y, COLOR_DARKBLUE);
  bracket2[1] = rect(bracketrect1_w, bracketrect1_h, rightvertical_x - bracketrect1_w, bracket_y, COLOR_LIGHTBLUE);
  bracket2[2] = circ(bcirc_d, rightvertical_x - bcircshift + 1, bracket_y, COLOR_LIGHTBLUE);
  bracket2[3] = rect(vertical_w, bracketrect2_h, rightvertical_x, bracket_y + bracketrect1_h, COLOR_LIGHTBLUE);
  bracket2[4] = circ(scirc_d, rightvertical_x - scirc_d, bracket_y + bracketrect1_h, COLOR_BLACK);
  bracket2[5] = rect(stickout_w, stickout_h, rightvertical_x + vertical_w, stickout_y, COLOR_LIGHTBLUE);
  bracket2[6] = rect(vertical_w, smallrect_h, rightvertical_x, smallrect1_y, COLOR_ORANGE);
  bracket2[7] = rect(vertical_w, smallrect_h, rightvertical_x, smallrect2_y, COLOR_DARKGRAY);
  bracket2[8] = rect(vertical_w, bracketrect2_h, rightvertical_x, bottom1_y, COLOR_LIGHTBLUE);
  bracket2[9] = rect(stickout_w, stickout_h, rightvertical_x + vertical_w, stickout2_y, COLOR_LIGHTBLUE);
  bracket2[10] = circ(bcirc_d, rightvertical_x - bcircshift + 1, bottom2_y - bcircshift, COLOR_LIGHTBLUE);
  bracket2[11] = rect(bracketrect1_w, bracketrect1_h, rightvertical_x - bracketrect1_w, bottom2_y, COLOR_LIGHTBLUE);
  bracket2[12] = circ(scirc_d, rightvertical_x - scirc_d, bottom2_y - scircshift, COLOR_BLACK);

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
  lv_obj_align(stepIcon, bracket1[0], LV_ALIGN_OUT_RIGHT_MID, 13, 0);

  stepValue = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(stepValue, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, COLOR_STEPS);
  lv_label_set_text_static(stepValue, "0");
  lv_obj_align(stepValue, stepIcon, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  // menu buttons
  btnClose = lv_btn_create(lv_scr_act(), nullptr);
  btnClose->user_data = this;
  lv_obj_set_size(btnClose, 200, 60);
  lv_obj_align(btnClose, lv_scr_act(), LV_ALIGN_IN_BOTTOM_MID, 0, -15);
  lv_obj_set_style_local_bg_opa(btnClose, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_70);
  lv_obj_t* lblClose = lv_label_create(btnClose, nullptr);
  lv_label_set_text_static(lblClose, "X");
  lv_obj_set_event_cb(btnClose, event_handler);
  lv_obj_set_hidden(btnClose, true);

  btnSetUseSystemFont = lv_btn_create(lv_scr_act(), nullptr);
  btnSetUseSystemFont->user_data = this;
  lv_obj_set_size(btnSetUseSystemFont, 200, 60);
  lv_obj_align(btnSetUseSystemFont, lv_scr_act(), LV_ALIGN_IN_TOP_MID, 0, 15);
  lv_obj_set_style_local_bg_opa(btnSetUseSystemFont, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_70);
  const char* label_sysfont =
    settingsController.GetStarTrekUseSystemFont() ? WANT_SYSTEM_FONT : (starTrekFontAvailable ? WANT_ST_FONT : WANT_ST_FONT_BUT_NO);
  lblSetUseSystemFont = lv_label_create(btnSetUseSystemFont, nullptr);
  lv_label_set_text_static(lblSetUseSystemFont, label_sysfont);
  lv_obj_set_event_cb(btnSetUseSystemFont, event_handler);
  lv_obj_set_hidden(btnSetUseSystemFont, true);

  btnSetAnimate = lv_btn_create(lv_scr_act(), nullptr);
  btnSetAnimate->user_data = this;
  lv_obj_set_size(btnSetAnimate, 200, 60);
  lv_obj_align(btnSetAnimate, lv_scr_act(), LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_local_bg_opa(btnSetAnimate, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_70);
  const char* label_animate = settingsController.GetStarTrekAnimate() ? WANT_ANIMATE : WANT_STATIC;
  lblSetAnimate = lv_label_create(btnSetAnimate, nullptr);
  lv_label_set_text_static(lblSetAnimate, label_animate);
  lv_obj_set_event_cb(btnSetAnimate, event_handler);
  lv_obj_set_hidden(btnSetAnimate, true);
}

WatchFaceStarTrek::~WatchFaceStarTrek() {
  lv_task_del(taskRefresh);

  if (starTrekFontAvailable && !settingsController.GetStarTrekUseSystemFont() && font_time != nullptr) {
    printf("use system: %b\n", font_time == &jetbrains_mono_extrabold_compressed);
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
    auto hour = dateTimeController.Hours();
    auto minute = dateTimeController.Minutes();
    auto year = dateTimeController.Year();
    auto month = dateTimeController.Month();
    auto dayOfWeek = dateTimeController.DayOfWeek();
    auto day = dateTimeController.Day();

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
  if (stepCount.IsUpdated()) {
    lv_label_set_text_fmt(stepValue, "%lu", stepCount.Get());
    lv_obj_realign(stepValue);
  }

  if (!lv_obj_get_hidden(btnClose)) {
    if ((settingsAutoCloseTick > 0) && (lv_tick_get() - settingsAutoCloseTick > SETTINGS_AUTO_CLOSE_TICKS)) {
      lv_obj_set_hidden(btnClose, true);
      lv_obj_set_hidden(btnSetUseSystemFont, true);
      settingsAutoCloseTick = 0;
    }
  }

  if (settingsController.GetStarTrekAnimate()) {
    if (animateType == AnimateType::Start && lv_tick_get() - animatorTick > ANIMATOR_START_TICKS) {
      animateStartStep();
    }
    // else if (animateType == AnimateType::TODO) {
    // }
  }
}

void WatchFaceStarTrek::animateStartStep() {
  switch (animateStage) {
    case 0:
      lv_obj_set_hidden(topRightRect, false);
      lv_obj_set_hidden(upperShapeRect1, false);
      lv_obj_set_hidden(upperShapeRect2, false);
      lv_obj_set_hidden(upperShapeRect3, false);
      lv_obj_set_hidden(upperShapeRect4, false);
      lv_obj_set_hidden(upperShapeCirc1, false);
      lv_obj_set_hidden(upperShapeCirc2, false);
      break;
    case 1:
      lv_obj_set_hidden(dateRect1, false);
      lv_obj_set_hidden(iconRect1, false);
      lv_obj_set_hidden(iconCirc1, false);
      lv_obj_set_hidden(label_dayname, false);
      batteryIcon.SetVisible(true);
      break;
    case 2:
      lv_obj_set_hidden(dateRect2, false);
      lv_obj_set_hidden(iconRect2, false);
      lv_obj_set_hidden(iconCirc2, false);
      lv_obj_set_hidden(label_day, false);
      lv_obj_set_hidden(notificationIcon, false);
      break;
    case 3:
      lv_obj_set_hidden(dateRect3, false);
      lv_obj_set_hidden(iconRect3, false);
      lv_obj_set_hidden(iconCirc3, false);
      lv_obj_set_hidden(label_month, false);
      lv_obj_set_hidden(bleIcon, false);
      break;
    case 4:
      lv_obj_set_hidden(dateRect4, false);
      lv_obj_set_hidden(iconRect4, false);
      lv_obj_set_hidden(iconCirc4, false);
      lv_obj_set_hidden(label_year, false);
      lv_obj_set_hidden(batteryPlug, false);
      break;
    case 5:
      lv_obj_set_hidden(lowerShapeCirc1, false);
      lv_obj_set_hidden(lowerShapeCircHalfCut, false);
      lv_obj_set_hidden(bottomRightRect, false);
      lv_obj_set_hidden(lowerShapeRect1, false);
      lv_obj_set_hidden(lowerShapeRect2, false);
      lv_obj_set_hidden(lowerShapeRect3, false);
      lv_obj_set_hidden(lowerShapeRect4, false);
      lv_obj_set_hidden(lowerShapeCirc2, false);
      break;
    case 6:
      lv_obj_set_hidden(bar1, false);
      lv_obj_set_hidden(bar2, false);
      for (uint8_t i = 0; i < 13; i++) {
        lv_obj_set_hidden(bracket1[i], false);
        lv_obj_set_hidden(bracket2[i], false);
      }
      lv_obj_set_hidden(heartbeatIcon, false);
      lv_obj_set_hidden(heartbeatValue, false);
      lv_obj_set_hidden(stepIcon, false);
      lv_obj_set_hidden(stepValue, false);
      break;
  }
  animateStage++;
  animatorTick = lv_tick_get();
}

void WatchFaceStarTrek::setVisible(bool visible) {
  lv_obj_set_hidden(topRightRect, !visible);
  lv_obj_set_hidden(upperShapeRect1, !visible);
  lv_obj_set_hidden(upperShapeRect2, !visible);
  lv_obj_set_hidden(upperShapeRect3, !visible);
  lv_obj_set_hidden(upperShapeRect4, !visible);
  lv_obj_set_hidden(upperShapeCirc1, !visible);
  lv_obj_set_hidden(upperShapeCirc2, !visible);
  lv_obj_set_hidden(dateRect1, !visible);
  lv_obj_set_hidden(iconRect1, !visible);
  lv_obj_set_hidden(iconCirc1, !visible);
  lv_obj_set_hidden(label_dayname, !visible);
  lv_obj_set_hidden(dateRect2, !visible);
  lv_obj_set_hidden(iconRect2, !visible);
  lv_obj_set_hidden(iconCirc2, !visible);
  lv_obj_set_hidden(label_day, !visible);
  lv_obj_set_hidden(notificationIcon, !visible);
  lv_obj_set_hidden(dateRect3, !visible);
  lv_obj_set_hidden(iconRect3, !visible);
  lv_obj_set_hidden(iconCirc3, !visible);
  lv_obj_set_hidden(label_month, !visible);
  lv_obj_set_hidden(bleIcon, !visible);
  lv_obj_set_hidden(dateRect4, !visible);
  lv_obj_set_hidden(iconRect4, !visible);
  lv_obj_set_hidden(iconCirc4, !visible);
  lv_obj_set_hidden(label_year, !visible);
  lv_obj_set_hidden(batteryPlug, !visible);
  lv_obj_set_hidden(lowerShapeCirc1, !visible);
  lv_obj_set_hidden(lowerShapeCircHalfCut, !visible);
  lv_obj_set_hidden(bottomRightRect, !visible);
  lv_obj_set_hidden(lowerShapeRect1, !visible);
  lv_obj_set_hidden(lowerShapeRect2, !visible);
  lv_obj_set_hidden(lowerShapeRect3, !visible);
  lv_obj_set_hidden(lowerShapeRect4, !visible);
  lv_obj_set_hidden(lowerShapeCirc2, !visible);
  lv_obj_set_hidden(bar1, !visible);
  lv_obj_set_hidden(bar2, !visible);
  lv_obj_set_hidden(heartbeatIcon, !visible);
  lv_obj_set_hidden(heartbeatValue, !visible);
  lv_obj_set_hidden(stepIcon, !visible);
  lv_obj_set_hidden(stepValue, !visible);
  batteryIcon.SetVisible(visible);
  for (uint8_t i = 0; i < 13; i++) {
    lv_obj_set_hidden(bracket1[i], !visible);
    lv_obj_set_hidden(bracket2[i], !visible);
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

bool WatchFaceStarTrek::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
  if ((event == Pinetime::Applications::TouchEvents::LongTap) && lv_obj_get_hidden(btnClose)) {
    lv_obj_set_hidden(btnClose, false);
    lv_obj_set_hidden(btnSetUseSystemFont, false);
    lv_obj_set_hidden(btnSetAnimate, false);
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
    lv_obj_set_hidden(btnClose, true);
    lv_obj_set_hidden(btnSetUseSystemFont, true);
    lv_obj_set_hidden(btnSetAnimate, true);
    return true;
  }
  return false;
}

void WatchFaceStarTrek::UpdateSelected(lv_obj_t* object, lv_event_t event) {
  if (event == LV_EVENT_CLICKED) {

    settingsAutoCloseTick = lv_tick_get();

    if (object == btnClose) {
      lv_obj_set_hidden(btnClose, true);
      lv_obj_set_hidden(btnSetUseSystemFont, true);
      lv_obj_set_hidden(btnSetAnimate, true);
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
      if (settingsController.GetStarTrekAnimate()) {
        settingsController.SetStarTrekAnimate(false);
        setVisible(true);
      } else {
        settingsController.SetStarTrekAnimate(true);
        startAnimation();
      }
      lv_label_set_text_static(lblSetAnimate, settingsController.GetStarTrekAnimate() ? WANT_ANIMATE : WANT_STATIC);
    }
  }
}

void WatchFaceStarTrek::OnLCDWakeup() {
  if (settingsController.GetStarTrekAnimate()) {
    startAnimation();
  }
}

void WatchFaceStarTrek::startAnimation() {
  animateType = AnimateType::Start;
  setVisible(false);
  animatorTick = lv_tick_get();
  animateStage = 0;
}