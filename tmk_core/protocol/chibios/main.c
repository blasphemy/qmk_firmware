/*
 * (c) 2015 flabberast <s3+flabbergast@sdfeu.org>
 *
 * Based on the following work:
 *  - Guillaume Duc's raw hid example (MIT License)
 *    https://github.com/guiduc/usb-hid-chibios-example
 *  - PJRC Teensy examples (MIT License)
 *    https://www.pjrc.com/teensy/usb_keyboard.html
 *  - hasu's TMK keyboard code (GPL v2 and some code Modified BSD)
 *    https://github.com/tmk/tmk_keyboard/
 *  - ChibiOS demo code (Apache 2.0 License)
 *    http://www.chibios.org
 *
 * Since some GPL'd code is used, this work is licensed under
 * GPL v2 or later.
 */

#include "ch.h"
#include "hal.h"

#include "usb_main.h"

/* TMK includes */
#include "report.h"
#include "host.h"
#include "host_driver.h"
#include "keyboard.h"
#include "action.h"
#include "action_util.h"
#include "mousekey.h"
#include "led.h"
#include "sendchar.h"
#include "debug.h"
#include "printf.h"
#ifdef SLEEP_LED_ENABLE
#include "sleep_led.h"
#endif
#ifdef SERIAL_LINK_ENABLE
#include "serial_link/system/serial_link.h"
#endif
#ifdef VISUALIZER_ENABLE
#include "visualizer/visualizer.h"
#endif
#ifdef BLUETOOTH_ENABLE
#include "bluetooth.h"
#endif
#include "suspend.h"


/* -------------------------
 *   TMK host driver defs
 * -------------------------
 */

/* declarations */
uint8_t keyboard_leds(void);
void send_keyboard(report_keyboard_t *report);
void send_mouse(report_mouse_t *report);
void send_system(uint16_t data);
void send_consumer(uint16_t data);

/* host struct */
host_driver_t chibios_driver = {
  keyboard_leds,
  send_keyboard,
  send_mouse,
  send_system,
  send_consumer
};


/* TESTING
 * Amber LED blinker thread, times are in milliseconds.
 */
/* set this variable to non-zero anywhere to blink once */
// uint8_t blinkLed = 0;
// static THD_WORKING_AREA(waBlinkerThread, 128);
// static THD_FUNCTION(blinkerThread, arg) {
//   (void)arg;
//   chRegSetThreadName("blinkOrange");
//   while(true) {
//     if(blinkLed) {
//       blinkLed = 0;
//       palSetPad(TEENSY_PIN13_IOPORT, TEENSY_PIN13);
//       chThdSleepMilliseconds(100);
//       palClearPad(TEENSY_PIN13_IOPORT, TEENSY_PIN13);
//     }
//     chThdSleepMilliseconds(100);
//   }
// }



/* Main thread
 */
int main(void) {
  /* ChibiOS/RT init */
  halInit();
  chSysInit();

  // TESTING
  // chThdCreateStatic(waBlinkerThread, sizeof(waBlinkerThread), NORMALPRIO, blinkerThread, NULL);

  /* Init USB */
  init_usb_driver(&USB_DRIVER);

  /* init printf */
  init_printf(NULL,sendchar_pf);

#ifdef SERIAL_LINK_ENABLE
  init_serial_link();
#endif

#ifdef VISUALIZER_ENABLE
  visualizer_init();
#endif

  host_driver_t* driver = NULL;

  /* Wait until the USB or serial link is active */
  #ifndef BLUETOOTH_ENABLE
  while (true) {
    if(USB_DRIVER.state == USB_ACTIVE) {
      driver = &chibios_driver;
      break;
    }
#ifdef SERIAL_LINK_ENABLE
    if(is_serial_link_connected()) {
      driver = get_serial_link_driver();
      break;
    }
    serial_link_update();
#endif
    chThdSleepMilliseconds(50);
  }
  #else
  bluefruit_init_serial_link();
  driver = &chibios_driver;
  #endif

  /* Do need to wait here!
   * Otherwise the next print might start a transfer on console EP
   * before the USB is completely ready, which sometimes causes
   * HardFaults.
   */
  chThdSleepMilliseconds(50);

  print("USB configured.\n");

  /* init TMK modules */
  keyboard_init();
  host_set_driver(driver);

#ifdef SLEEP_LED_ENABLE
  sleep_led_init();
#endif

  print("Keyboard start.\n");

  /* Main loop */
  while(true) {
    #ifndef BLUETOOTH_ENABLE
    if(USB_DRIVER.state == USB_SUSPENDED) {
      print("[s]");
#ifdef VISUALIZER_ENABLE
      visualizer_suspend();
#endif
      while(USB_DRIVER.state == USB_SUSPENDED) {
        /* Do this in the suspended state */
#ifdef SERIAL_LINK_ENABLE
        serial_link_update();
#endif
        suspend_power_down(); // on AVR this deep sleeps for 15ms
        /* Remote wakeup */
        if((USB_DRIVER.status & 2) && suspend_wakeup_condition()) {
          send_remote_wakeup(&USB_DRIVER);
        }
      }
      /* Woken up */
      // variables has been already cleared by the wakeup hook
      send_keyboard_report();
#ifdef MOUSEKEY_ENABLE
      mousekey_send();
#endif /* MOUSEKEY_ENABLE */

#ifdef VISUALIZER_ENABLE
      visualizer_resume();
#endif
    }
#endif
    keyboard_task();
  }
}
