#include "displayapp/screens/Notifications.h"
#include "displayapp/screens/NotificationIcon.h"
#include "displayapp/DisplayApp.h"
#include "components/ble/MusicService.h"
#include "components/ble/AlertNotificationService.h"
#include "displayapp/screens/Symbols.h"
#include <algorithm>
#include "displayapp/InfiniTimeTheme.h"

using namespace Pinetime::Applications::Screens;
extern lv_font_t jetbrains_mono_extrabold_compressed;
extern lv_font_t jetbrains_mono_bold_20;

Notifications::Notifications(DisplayApp* app,
                             Pinetime::Controllers::NotificationManager& notificationManager,
                             Pinetime::Controllers::AlertNotificationService& alertNotificationService,
                             Pinetime::Controllers::MotorController& motorController,
                             Pinetime::Controllers::DateTime& dateTimeController,
                             System::SystemTask& systemTask,
                             Modes mode)
  : app {app},
    notificationManager {notificationManager},
    alertNotificationService {alertNotificationService},
    dateTimeController {dateTimeController},
    motorController {motorController},
    systemTask {systemTask},
    mode {mode} {

  notificationManager.ClearNewNotificationFlag();
  auto notification = notificationManager.GetLastNotification();
  if (notification.valid) {
    currentId = notification.id;
    currentItem = std::make_unique<NotificationItem>(notification.Title(),
                                                     notification.Message(),
                                                     1,
                                                     notification.category,
                                                     notification.timeArrived,
                                                     std::chrono::system_clock::to_time_t(this->dateTimeController.CurrentDateTime()),
                                                     notificationManager.NbNotifications(),
                                                     alertNotificationService,
                                                     motorController,
                                                     notificationManager,
                                                     this);
    validDisplay = true;
  } else {
    currentItem = std::make_unique<NotificationItem>(alertNotificationService, motorController, notificationManager, this);
    validDisplay = false;
  }
  if (mode == Modes::Preview) {
    systemTask.PushMessage(System::Messages::DisableSleeping);
    if (notification.category == Controllers::NotificationManager::Categories::IncomingCall) {
      motorController.StartRinging();
    } else {
      motorController.RunForDuration(35);
    }

    timeoutLine = lv_line_create(lv_scr_act(), nullptr);

    lv_obj_set_style_local_line_width(timeoutLine, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, 3);
    lv_obj_set_style_local_line_color(timeoutLine, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_style_local_line_rounded(timeoutLine, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, true);

    lv_line_set_points(timeoutLine, timeoutLinePoints, 2);
    timeoutTickCountStart = xTaskGetTickCount();
    interacted = false;
  }

  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
}

Notifications::~Notifications() {
  lv_task_del(taskRefresh);
  // make sure we stop any vibrations before exiting
  motorController.StopRinging();
  systemTask.PushMessage(System::Messages::EnableSleeping);
  lv_obj_clean(lv_scr_act());
}

void Notifications::Refresh() {
  if (mode == Modes::Preview && timeoutLine != nullptr) {
    TickType_t tick = xTaskGetTickCount();
    int32_t pos = LV_HOR_RES - ((tick - timeoutTickCountStart) / (timeoutLength / LV_HOR_RES));
    if (pos <= 0) {
      running = false;
    } else {
      timeoutLinePoints[1].x = pos;
      lv_line_set_points(timeoutLine, timeoutLinePoints, 2);
    }

  } else if (mode == Modes::Preview && dismissingNotification) {
    running = false;
    currentItem = std::make_unique<NotificationItem>(alertNotificationService, motorController, this);

  } else if (dismissingNotification) {
    dismissingNotification = false;
    auto notification = notificationManager.Get(currentId);
    if (!notification.valid) {
      notification = notificationManager.GetLastNotification();
    }
    currentId = notification.id;

    if (!notification.valid) {
      validDisplay = false;
    }

    currentItem.reset(nullptr);
    if (afterDismissNextMessageFromAbove) {
      app->SetFullRefresh(DisplayApp::FullRefreshDirections::Down);
    } else {
      app->SetFullRefresh(DisplayApp::FullRefreshDirections::Up);
    }

    if (validDisplay) {
      Controllers::NotificationManager::Notification::Idx currentIdx = notificationManager.IndexOf(currentId);
      currentItem = std::make_unique<NotificationItem>(notification.Title(),
                                                       notification.Message(),
                                                       currentIdx + 1,
                                                       notification.category,
                                                       notification.timeArrived,
                                                       std::chrono::system_clock::to_time_t(this->dateTimeController.CurrentDateTime()),
                                                       notificationManager.NbNotifications(),
                                                       alertNotificationService,
                                                       motorController,
                                                       notificationManager,
                                                       this);
    } else {
      running = false;
    }
  }

  running = running && currentItem->IsRunning();
}

void Notifications::OnPreviewInteraction() {
  systemTask.PushMessage(System::Messages::EnableSleeping);
  motorController.StopRinging();
  if (timeoutLine != nullptr) {
    lv_obj_del(timeoutLine);
    timeoutLine = nullptr;
  }
}

void Notifications::DismissToBlack() {
  currentItem.reset(nullptr);
  app->SetFullRefresh(DisplayApp::FullRefreshDirections::RightAnim);
  // create black transition screen to let the notification dismiss to blackness
  lv_obj_t* blackBox = lv_obj_create(lv_scr_act(), nullptr);
  lv_obj_set_size(blackBox, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_local_bg_color(blackBox, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  dismissingNotification = true;
}

void Notifications::DismissAll() {
  if (validDisplay) {
    auto previousMessage = notificationManager.GetPrevious(currentId);
    auto nextMessage = notificationManager.GetNext(currentId);
    while (notificationManager.NbNotifications() != 0) {
      previousMessage = notificationManager.GetPrevious(currentId);
      nextMessage = notificationManager.GetNext(currentId);
      afterDismissNextMessageFromAbove = previousMessage.valid;
      notificationManager.Dismiss(currentId);
      if (previousMessage.valid) {
        currentId = previousMessage.id;
      } else if (nextMessage.valid) {
        currentId = nextMessage.id;
      }
    }
    DismissToBlack();
  }
}

void Notifications::OnPreviewDismiss() {
  notificationManager.Dismiss(currentId);
  if (timeoutLine != nullptr) {
    lv_obj_del(timeoutLine);
    timeoutLine = nullptr;
  }
  DismissToBlack();
}

bool Notifications::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
  if (mode != Modes::Normal) {
    if (!interacted && event == TouchEvents::Tap) {
      interacted = true;
      OnPreviewInteraction();
      return true;
    } else if (event == Pinetime::Applications::TouchEvents::SwipeRight) {
      OnPreviewDismiss();
      return true;
    }
    return false;
  }

  switch (event) {
    case Pinetime::Applications::TouchEvents::SwipeRight:
      if (dismissMenuOpen) {
        currentItem->hideClearBtn();
        app->SetFullRefresh(DisplayApp::FullRefreshDirections::Left);
        dismissMenuOpen = false;
      } else if (validDisplay) {
        auto previousMessage = notificationManager.GetPrevious(currentId);
        auto nextMessage = notificationManager.GetNext(currentId);
        afterDismissNextMessageFromAbove = previousMessage.valid;
        notificationManager.Dismiss(currentId);
        if (previousMessage.valid) {
          currentId = previousMessage.id;
        } else if (nextMessage.valid) {
          currentId = nextMessage.id;
        } else {
          // don't update id, won't be found be refresh and try to load latest message or no message box
        }
        DismissToBlack();
        return true;
      }
      return false;
    case Pinetime::Applications::TouchEvents::SwipeLeft:
      if (notificationManager.NbNotifications() == 0) {
        return false;
      }
      currentItem->showClearBtn();
      app->SetFullRefresh(DisplayApp::FullRefreshDirections::Right);
      dismissMenuOpen = true;
      return true;
    case Pinetime::Applications::TouchEvents::SwipeDown: {
      Controllers::NotificationManager::Notification previousNotification;
      if (validDisplay) {
        previousNotification = notificationManager.GetPrevious(currentId);
      } else {
        previousNotification = notificationManager.GetLastNotification();
      }

      if (!previousNotification.valid) {
        return true;
      }

      currentId = previousNotification.id;
      Controllers::NotificationManager::Notification::Idx currentIdx = notificationManager.IndexOf(currentId);
      validDisplay = true;
      currentItem.reset(nullptr);
      app->SetFullRefresh(DisplayApp::FullRefreshDirections::Down);
      currentItem = std::make_unique<NotificationItem>(previousNotification.Title(),
                                                       previousNotification.Message(),
                                                       currentIdx + 1,
                                                       previousNotification.category,
                                                       previousNotification.timeArrived,
                                                       std::chrono::system_clock::to_time_t(this->dateTimeController.CurrentDateTime()),
                                                       notificationManager.NbNotifications(),
                                                       alertNotificationService,
                                                       motorController,
                                                       notificationManager,
                                                       this);
    }
      return true;
    case Pinetime::Applications::TouchEvents::SwipeUp: {
      Controllers::NotificationManager::Notification nextNotification;
      if (validDisplay) {
        nextNotification = notificationManager.GetNext(currentId);
      } else {
        nextNotification = notificationManager.GetLastNotification();
      }

      if (!nextNotification.valid) {
        running = false;
        return false;
      }

      currentId = nextNotification.id;
      Controllers::NotificationManager::Notification::Idx currentIdx = notificationManager.IndexOf(currentId);
      validDisplay = true;
      currentItem.reset(nullptr);
      app->SetFullRefresh(DisplayApp::FullRefreshDirections::Up);
      currentItem = std::make_unique<NotificationItem>(nextNotification.Title(),
                                                       nextNotification.Message(),
                                                       currentIdx + 1,
                                                       nextNotification.category,
                                                       previousNotification.timeArrived,
                                                       std::chrono::system_clock::to_time_t(this->dateTimeController.CurrentDateTime()),
                                                       notificationManager.NbNotifications(),
                                                       alertNotificationService,
                                                       motorController,
                                                       notificationManager,
                                                       this);
    }
      return true;
    default:
      return false;
  }
}

namespace {
  void CallEventHandler(lv_obj_t* obj, lv_event_t event) {
    auto* item = static_cast<Notifications::NotificationItem*>(obj->user_data);
    item->OnCallButtonEvent(obj, event);
  }
  void DismissEventHandler(lv_obj_t* obj, lv_event_t event) {
    auto* item = static_cast<Notifications::NotificationItem*>(obj->user_data);
    item->OnDismissButtonEvent(event);
  }
}

Notifications::NotificationItem::NotificationItem(Pinetime::Controllers::AlertNotificationService& alertNotificationService,
                                                  Pinetime::Controllers::MotorController& motorController,
                                                  Pinetime::Controllers::NotificationManager& notificationManager,
                                                  Notifications *parent)
  : NotificationItem("Notification",
                     "No notification to display",
                     0,
                     Controllers::NotificationManager::Categories::Unknown,
                     0,
                     0,
                     0,
                     alertNotificationService,
                     motorController,
                     notificationManager,
                     parent) {
}

Notifications::NotificationItem::NotificationItem(const char* title,
                                                  const char* msg,
                                                  uint8_t notifNr,
                                                  Controllers::NotificationManager::Categories category,
                                                  std::time_t timeArrived,
                                                  std::time_t timeNow,
                                                  uint8_t notifNb,
                                                  Pinetime::Controllers::AlertNotificationService& alertNotificationService,
                                                  Pinetime::Controllers::MotorController& motorController,
                                                  Pinetime::Controllers::NotificationManager& notificationManager,
                                                  Notifications *parent)
  : alertNotificationService {alertNotificationService}, motorController {motorController}, notificationManager {notificationManager}, parent {parent} {
  container = lv_cont_create(lv_scr_act(), nullptr);
  lv_obj_set_size(container, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_local_bg_color(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  lv_obj_set_style_local_pad_all(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_style_local_pad_inner(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_style_local_border_width(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);

  subject_container = lv_cont_create(container, nullptr);
  lv_obj_set_style_local_bg_color(subject_container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, Colors::bgAlt);
  lv_obj_set_style_local_pad_all(subject_container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 10);
  lv_obj_set_style_local_pad_inner(subject_container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 5);
  lv_obj_set_style_local_border_width(subject_container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);

  lv_obj_set_pos(subject_container, 0, 37);
  lv_obj_set_size(subject_container, LV_HOR_RES, LV_VER_RES - 66);
  lv_cont_set_layout(subject_container, LV_LAYOUT_COLUMN_LEFT);
  lv_cont_set_fit(subject_container, LV_FIT_NONE);

  // draw notification stack
  if (category != Pinetime::Controllers::NotificationManager::Categories::IncomingCall) {
    for (int i = 0; i < notifNb; i++) {
      lv_obj_t* alert_icon = lv_label_create(container, nullptr);
      if (i + 1 == notifNr) { // currently selected should be orange
        lv_obj_set_style_local_text_color(alert_icon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::orange);
      }
      // TODO: get categories for the other notifications
      lv_label_set_text_fmt(alert_icon, NotificationIcon::GetCategoryIcon(notificationManager.CategoryAt(i)));
      lv_obj_align(alert_icon, nullptr, LV_ALIGN_IN_BOTTOM_RIGHT, -5 + (-25 * i), -3);
    }
  }

  lv_obj_t* alert_count = lv_label_create(container, nullptr);
  lv_label_set_text_fmt(alert_count, "%i/%i", notifNr, notifNb);
  lv_obj_align(alert_count, nullptr, LV_ALIGN_IN_TOP_RIGHT, 0, 8);

  // almost impossible to receive a real notification at time 0, so skip because it is the "no notifications" notification
  if (timeNow != 0) {
    auto diff = std::chrono::system_clock::from_time_t(timeNow) - std::chrono::system_clock::from_time_t(timeArrived);
    std::chrono::minutes age = std::chrono::duration_cast<std::chrono::minutes>(diff);
    uint32_t ageInt = static_cast<uint32_t>(age.count());
    char timeUnit;
    if (ageInt >= 60 * 24) {
      ageInt /= (60 * 24);
      timeUnit = 'd';
    } else if (ageInt >= 60) {
      ageInt /= 60;
      timeUnit = 'h';
    } else {
      timeUnit = 'm';
    }
    lv_obj_t* alert_age = lv_label_create(container, nullptr);
    lv_label_set_text_fmt(alert_age, "%d%c ago", ageInt, timeUnit);
    // same format as alert_count
    lv_obj_align(alert_age, container, LV_ALIGN_IN_BOTTOM_LEFT, 5, -3);
  }

  lv_obj_t* alert_type = lv_label_create(container, nullptr);
  lv_obj_set_style_local_text_color(alert_type, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::orange);
  if (title == nullptr) {
    lv_label_set_text_static(alert_type, "Notification");
  } else {
    // copy title to label and replace newlines with spaces
    lv_label_set_text(alert_type, title);
    char* pchar = strchr(lv_label_get_text(alert_type), '\n');
    while (pchar != nullptr) {
      *pchar = ' ';
      pchar = strchr(pchar + 1, '\n');
    }
    lv_label_refr_text(alert_type);
  }
  lv_label_set_long_mode(alert_type, LV_LABEL_LONG_SROLL_CIRC);
  lv_obj_set_width(alert_type, 180);
  lv_obj_align(alert_type, nullptr, LV_ALIGN_IN_TOP_LEFT, 0, 8);

  lv_obj_t* alert_subject = lv_label_create(subject_container, nullptr);
  lv_label_set_long_mode(alert_subject, LV_LABEL_LONG_BREAK);
  lv_obj_set_width(alert_subject, LV_HOR_RES - 20);

  btnClearAll = lv_btn_create(lv_scr_act(), nullptr);
  btnClearAll->user_data = this;
  lv_obj_set_size(btnClearAll, 100, 100);
  lv_obj_align(btnClearAll, lv_scr_act(), LV_ALIGN_IN_RIGHT_MID, 0, 0);
  lv_obj_set_style_local_bg_color(btnClearAll, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_RED);
  lv_obj_set_style_local_pad_all(btnClearAll, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 10);
  lv_obj_set_style_local_pad_inner(btnClearAll, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 5);
  lv_obj_set_style_local_border_width(btnClearAll, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_event_cb(btnClearAll, DismissEventHandler);
  labelBtnClearAll = lv_label_create(btnClearAll, nullptr);
  lv_label_set_text_static(labelBtnClearAll, "Dismiss\n  All");
  lv_obj_set_hidden(btnClearAll, true);


  switch (category) {
    default:
      lv_label_set_text(alert_subject, msg);
      break;
    case Controllers::NotificationManager::Categories::IncomingCall: {
      lv_obj_set_height(subject_container, 108);
      lv_label_set_text_static(alert_subject, "Incoming call from");

      lv_obj_t* alert_caller = lv_label_create(subject_container, nullptr);
      lv_obj_align(alert_caller, alert_subject, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
      lv_label_set_long_mode(alert_caller, LV_LABEL_LONG_BREAK);
      lv_obj_set_width(alert_caller, LV_HOR_RES - 20);
      lv_label_set_text(alert_caller, msg);

      bt_accept = lv_btn_create(container, nullptr);
      bt_accept->user_data = this;
      lv_obj_set_event_cb(bt_accept, CallEventHandler);
      lv_obj_set_size(bt_accept, 76, 76);
      lv_obj_align(bt_accept, nullptr, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
      label_accept = lv_label_create(bt_accept, nullptr);
      lv_label_set_text_static(label_accept, Symbols::phone);
      lv_obj_set_style_local_bg_color(bt_accept, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::highlight);

      bt_reject = lv_btn_create(container, nullptr);
      bt_reject->user_data = this;
      lv_obj_set_event_cb(bt_reject, CallEventHandler);
      lv_obj_set_size(bt_reject, 76, 76);
      lv_obj_align(bt_reject, nullptr, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
      label_reject = lv_label_create(bt_reject, nullptr);
      lv_label_set_text_static(label_reject, Symbols::phoneSlash);
      lv_obj_set_style_local_bg_color(bt_reject, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_RED);

      bt_mute = lv_btn_create(container, nullptr);
      bt_mute->user_data = this;
      lv_obj_set_event_cb(bt_mute, CallEventHandler);
      lv_obj_set_size(bt_mute, 76, 76);
      lv_obj_align(bt_mute, nullptr, LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);
      label_mute = lv_label_create(bt_mute, nullptr);
      lv_label_set_text_static(label_mute, Symbols::volumMute);
      lv_obj_set_style_local_bg_color(bt_mute, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::lightGray);
    } break;
  }
}

void Notifications::NotificationItem::showClearBtn() {
  lv_obj_set_pos(container, -100, 0);
  lv_obj_set_hidden(btnClearAll, false);
}

void Notifications::NotificationItem::hideClearBtn() {
  lv_obj_set_pos(container, 0, 0);
  lv_obj_set_hidden(btnClearAll, true);
}

void Notifications::NotificationItem::OnCallButtonEvent(lv_obj_t* obj, lv_event_t event) {
  if (event != LV_EVENT_CLICKED) {
    return;
  }

  motorController.StopRinging();

  if (obj == bt_accept) {
    alertNotificationService.AcceptIncomingCall();
  } else if (obj == bt_reject) {
    alertNotificationService.RejectIncomingCall();
  } else if (obj == bt_mute) {
    alertNotificationService.MuteIncomingCall();
  }

  running = false;
}

void Notifications::NotificationItem::OnDismissButtonEvent(lv_event_t event) {
  if (event != LV_EVENT_CLICKED) {
    return;
  }

  this->parent->DismissAll();
}

Notifications::NotificationItem::~NotificationItem() {
  lv_obj_clean(lv_scr_act());
}
