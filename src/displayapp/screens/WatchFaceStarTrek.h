#pragma once

#include <displayapp/screens/BatteryIcon.h>
#include <lvgl/src/lv_core/lv_obj.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include "displayapp/screens/Screen.h"
#include "components/datetime/DateTimeController.h"
#include "components/ble/BleController.h"
#include "utility/DirtyValue.h"

constexpr uint8_t PART_COUNT_ICON_SPACE = 2;
constexpr uint8_t PART_COUNT_LIST_ITEM = 3;
constexpr uint8_t PART_COUNT_UPPER_SHAPE = 6;
constexpr uint8_t PART_COUNT_LOWER_SHAPE = 7;
constexpr uint8_t PART_COUNT_BRACKET = 13;

namespace Pinetime {
  namespace Controllers {
    class Settings;
    class Battery;
    class Ble;
    class NotificationManager;
    class HeartRateController;
    class MotionController;
  }

  namespace Applications {
    namespace Screens {

      class WatchFaceStarTrek : public Screen {
      public:
        WatchFaceStarTrek(Controllers::DateTime& dateTimeController,
                          const Controllers::Battery& batteryController,
                          const Controllers::Ble& bleController,
                          Controllers::NotificationManager& notificatioManager,
                          Controllers::Settings& settingsController,
                          Controllers::HeartRateController& heartRateController,
                          Controllers::MotionController& motionController,
                          Controllers::FS& filesystem);
        ~WatchFaceStarTrek() override;

        bool OnTouchEvent(TouchEvents event) override;
        bool OnButtonPushed() override;
        void UpdateSelected(lv_obj_t* object, lv_event_t event);

        void Refresh() override;
        void OnLCDWakeup() override;

      private:
        Controllers::DateTime& dateTimeController;
        const Controllers::Battery& batteryController;
        const Controllers::Ble& bleController;
        Controllers::NotificationManager& notificatioManager;
        Controllers::Settings& settingsController;
        Controllers::HeartRateController& heartRateController;
        Controllers::MotionController& motionController;
        Controllers::FS& filesystem;

        lv_task_t* taskRefresh;

        uint8_t displayedHour = -1;
        uint8_t displayedMinute = -1;
        uint16_t currentYear = 1970;
        Controllers::DateTime::Months currentMonth = Pinetime::Controllers::DateTime::Months::Unknown;
        Controllers::DateTime::Days currentDayOfWeek = Pinetime::Controllers::DateTime::Days::Unknown;
        uint8_t currentDay = 0;

        Utility::DirtyValue<uint8_t> batteryPercentRemaining {};
        Utility::DirtyValue<bool> powerPresent {};
        Utility::DirtyValue<bool> bleState {};
        Utility::DirtyValue<bool> bleRadioEnabled {};
        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>> currentDateTime {};
        Utility::DirtyValue<uint32_t> stepCount {};
        Utility::DirtyValue<uint8_t> heartbeat {};
        Utility::DirtyValue<bool> heartbeatRunning {};
        Utility::DirtyValue<bool> notificationState {};

        lv_obj_t* label_time_hour_1;
        lv_obj_t* label_time_hour_10;
        lv_obj_t* label_time_min_1;
        lv_obj_t* label_time_min_10;
        lv_obj_t* label_time_ampm;
        lv_obj_t* label_dayname;
        lv_obj_t* label_day;
        lv_obj_t* label_month;
        lv_obj_t* label_year;
        lv_obj_t* bleIcon;
        lv_obj_t* batteryPlug;
        lv_obj_t* heartbeatIcon;
        lv_obj_t* heartbeatValue;
        lv_obj_t* stepIcon;
        lv_obj_t* stepValue;
        lv_obj_t* notificationIcon;
        BatteryIcon batteryIcon;

        // background
        lv_obj_t *topRightRect, *bottomRightRect;
        lv_obj_t *bar1, *bar2;
        lv_obj_t* iconSpace[PART_COUNT_ICON_SPACE];
        lv_obj_t* listItem1[PART_COUNT_LIST_ITEM];
        lv_obj_t* listItem2[PART_COUNT_LIST_ITEM];
        lv_obj_t* listItem3[PART_COUNT_LIST_ITEM];
        lv_obj_t* listItem4[PART_COUNT_LIST_ITEM];
        lv_obj_t* upperShape[PART_COUNT_UPPER_SHAPE];
        lv_obj_t* lowerShape[PART_COUNT_LOWER_SHAPE];
        lv_obj_t* bracket1[PART_COUNT_BRACKET];
        lv_obj_t* bracket2[PART_COUNT_BRACKET];
        lv_obj_t *minuteAnchor, *hourAnchor;

        // config menu
        lv_obj_t* btnClose;
        lv_obj_t* lblClose;
        lv_obj_t* btnSetUseSystemFont;
        lv_obj_t* lblSetUseSystemFont;
        lv_obj_t* btnSetAnimate;
        lv_obj_t* lblSetAnimate;
        uint32_t settingsAutoCloseTick = 0;
        void setMenuButtonsVisible(bool visible);

        void drawWatchFace();
        lv_obj_t* rect(uint8_t w, uint8_t h, uint8_t x, uint8_t y, lv_color_t color);
        lv_obj_t* circ(uint8_t d, uint8_t x, uint8_t y, lv_color_t color);
        lv_obj_t* _base(uint8_t w, uint8_t h, uint8_t x, uint8_t y, lv_color_t color);
        lv_obj_t* label(lv_color_t color,
                        lv_obj_t* alignto = lv_scr_act(),
                        lv_align_t alignmode = LV_ALIGN_CENTER,
                        int16_t gapx = 0,
                        int16_t gapy = 0,
                        const char* text = "",
                        lv_obj_t* base = lv_scr_act());
        lv_obj_t* button(uint16_t sizex,
                         uint16_t sizey,
                         lv_obj_t* alignto = lv_scr_act(),
                         lv_align_t alignmode = LV_ALIGN_CENTER,
                         int16_t gapx = 0,
                         int16_t gapy = 0);

        lv_font_t* font_time = nullptr;
        bool starTrekFontAvailable = false;
        void updateFontTime();

        enum class AnimateType { Start };
        AnimateType animateType = AnimateType::Start;
        uint32_t animatorTick = 0;
        uint8_t animateStage = 0;
        void setVisible(bool visible);
        void setShapeVisible(lv_obj_t** shape, uint8_t partcount, bool visible);
        void animateStartStep();
        void startAnimation();
      };
    }
  }
}