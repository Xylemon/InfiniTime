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
        uint8_t displayedHour = -1;
        uint8_t displayedMinute = -1;

        uint16_t currentYear = 1970;
        Controllers::DateTime::Months currentMonth = Pinetime::Controllers::DateTime::Months::Unknown;
        Controllers::DateTime::Days currentDayOfWeek = Pinetime::Controllers::DateTime::Days::Unknown;
        uint8_t currentDay = 0;
        uint32_t settingsAutoCloseTick = 0;

        Utility::DirtyValue<uint8_t> batteryPercentRemaining {};
        Utility::DirtyValue<bool> powerPresent {};
        Utility::DirtyValue<bool> bleState {};
        Utility::DirtyValue<bool> bleRadioEnabled {};
        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>> currentDateTime {};
        Utility::DirtyValue<bool> motionSensorOk {};
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

        // background
        lv_obj_t *topRightRect, *bottomRightRect;
        lv_obj_t *bar1, *bar2;
        lv_obj_t *dateRect1, *dateRect2, *dateRect3, *dateRect4;
        lv_obj_t *iconRect1, *iconRect2, *iconRect3, *iconRect4;
        lv_obj_t *iconCirc1, *iconCirc2, *iconCirc3, *iconCirc4;
        lv_obj_t *upperShapeRect1, *upperShapeRect2, *upperShapeRect3, *upperShapeRect4, *upperShapeCirc1, *upperShapeCirc2;
        lv_obj_t *lowerShapeRect1, *lowerShapeRect2, *lowerShapeRect3, *lowerShapeRect4, *lowerShapeCirc1, *lowerShapeCirc2,
          *lowerShapeCircHalfCut;
        lv_obj_t *minuteAnchor, *hourAnchor;
        lv_obj_t* bracket1[13];
        lv_obj_t* bracket2[13];

        lv_obj_t* btnClose;
        lv_obj_t* btnSetUseSystemFont;
        lv_obj_t* lblSetUseSystemFont;
        lv_obj_t* btnSetAnimate;
        lv_obj_t* lblSetAnimate;

        BatteryIcon batteryIcon;

        Controllers::DateTime& dateTimeController;
        const Controllers::Battery& batteryController;
        const Controllers::Ble& bleController;
        Controllers::NotificationManager& notificatioManager;
        Controllers::Settings& settingsController;
        Controllers::HeartRateController& heartRateController;
        Controllers::MotionController& motionController;

        lv_task_t* taskRefresh;

        lv_obj_t* rect(uint8_t w, uint8_t h, uint8_t x, uint8_t y, lv_color_t color);
        lv_obj_t* circ(uint8_t d, uint8_t x, uint8_t y, lv_color_t color);
        lv_obj_t* _base(uint8_t w, uint8_t h, uint8_t x, uint8_t y, lv_color_t color);
        void drawWatchFace();
        void updateFontTime();

        lv_font_t* font_time = nullptr;
        bool starTrekFontAvailable = false;

        enum class AnimateType {Start};
        AnimateType animateType = AnimateType::Start;
        uint32_t animatorTick = 0;
        uint8_t animateStage = 0;
        void setVisible(bool visible);
        void animateStartStep();
        void startAnimation();
      };
    }
  }
}
