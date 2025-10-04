#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main()
{
    libusb_context* ctx;
    int res = libusb_init(&ctx);
    printf("Return value from libusb_init: %d\n", res);

    libusb_device** devs;
    // pointer to pointer of device, used to retrieve a list of devices
    ssize_t count = libusb_get_device_list(ctx, &devs);  // get the list of devices
    if (count < 0)
        return -1;
    printf("Devices detected: %d\n", count);
    for (int i = 0; i < count; i++) {
      struct libusb_device_descriptor desc;
      res = libusb_get_device_descriptor(devs[i], &desc);
      printf(
"USB device %d:\n\
uint8_t  bLength=%X; \n\
\n\
/** Descriptor type. Will have value\n\
 * ref libusb_descriptor_type::LIBUSB_DT_DEVICE LIBUSB_DT_DEVICE in this\n\
 * context. */\n\
uint8_t  bDescriptorType=%X;\n\
\n\
/** USB specification release number in binary-coded decimal. A value of\n\
 * 0x0200 indicates USB 2.0, 0x0110 indicates USB 1.1, etc. */\n\
uint16_t bcdUSB=%X;\n\
\n\
/** USB-IF class code for the device. See \ref libusb_class_code. */\n\
uint8_t  bDeviceClass=%X;\n\
\n\
/** USB-IF subclass code for the device, qualified by the bDeviceClass\n\
 * value */\n\
uint8_t  bDeviceSubClass=%X;\n\
\n\
/** USB-IF protocol code for the device, qualified by the bDeviceClass and\n\
 * bDeviceSubClass values */\n\
uint8_t  bDeviceProtocol=%X;\n\
\n\
/** Maximum packet size for endpoint 0 */\n\
uint8_t  bMaxPacketSize0=%X;\n\
\n\
/** USB-IF vendor ID */\n\
uint16_t idVendor=%X;\n\
\n\
/** USB-IF product ID */\n\
uint16_t idProduct=%X;\n\
\n\
/** Device release number in binary-coded decimal */\n\
uint16_t bcdDevice=%X;\n\
\n\
/** Index of string descriptor describing manufacturer */\n\
uint8_t  iManufacturer=%X; \n\
\n\
/** Index of string descriptor describing product */\n\
uint8_t  iProduct=%X;\n\
\n\
/** Index of string descriptor containing device serial number */\n\
uint8_t  iSerialNumber=%X;\n\
\n\
/** Number of possible configurations */\n\
uint8_t  bNumConfigurations=%X;",
          i, 
          desc.bLength,
          desc.bDescriptorType,
          desc.bcdUSB,
          desc.bDeviceClass,
          desc.bDeviceSubClass,
          desc.bDeviceProtocol,
          desc.bMaxPacketSize0,
          desc.idVendor,
          desc.idProduct,
          desc.bcdDevice,
          desc.iManufacturer,
          desc.iProduct,
          desc.iSerialNumber,
          desc.bNumConfigurations);
    }

      return 0;
    }
