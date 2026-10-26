#include <admin.h>

#include <string.h>
#include <usb_device.h>

/* Override the function defined in usb_device.c */
void usb_resources_alloc(void) {
  uint8_t iface = 0;
  uint8_t ep = 1;

  memset(&IFACE_TABLE, 0xFF, sizeof(IFACE_TABLE));
  memset(&EP_TABLE, 0xFF, sizeof(EP_TABLE));
  memset(&EP_SIZE_TABLE, 0, sizeof(EP_SIZE_TABLE));

  EP_TABLE.ctap_hid = ep++;
  IFACE_TABLE.ctap_hid = iface++;
  EP_SIZE_TABLE.ctap_hid = 64;

  IFACE_TABLE.webusb = iface++;

  EP_TABLE.ccid = ep++;
  IFACE_TABLE.ccid = iface++;
  EP_SIZE_TABLE.ccid = 64;

  //if (cfg_is_kbd_interface_enable()) {
    EP_TABLE.kbd_hid = ep;
    IFACE_TABLE.kbd_hid = iface;
    EP_SIZE_TABLE.kbd_hid = 8;
  //}

}
