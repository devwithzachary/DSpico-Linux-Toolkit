#include "tusb.h"
#include <string.h>

#define DSPICO_USB_VID  0x1209
#define DSPICO_USB_PID  0xD501

enum
{
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_HID_KBD,
    ITF_NUM_HID_MOUSE,
    ITF_NUM_HID_CONSUMER,
    ITF_NUM_TOTAL
};

#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82
#define EPNUM_HID_KBD     0x83
#define EPNUM_HID_MOUSE   0x84
#define EPNUM_HID_CONS    0x85

static tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = DSPICO_USB_VID,
    .idProduct          = DSPICO_USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

static uint8_t const desc_hid_report_kbd[] =
{
    TUD_HID_REPORT_DESC_KEYBOARD()
};

static uint8_t const desc_hid_report_mouse[] =
{
    TUD_HID_REPORT_DESC_MOUSE()
};

static uint8_t const desc_hid_report_consumer[] =
{
    TUD_HID_REPORT_DESC_CONSUMER()
};

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
    switch (instance)
    {
        case 0: return desc_hid_report_kbd;
        case 1: return desc_hid_report_mouse;
        case 2: return desc_hid_report_consumer;
        default: return NULL;
    }
}

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + (3 * TUD_HID_DESC_LEN))

static uint8_t const desc_configuration[] =
{
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    /* Interface 0/1: CDC ACM Serial (/dev/ttyACM0) */
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

    /* Interface 2: Boot Keyboard (instance 0) */
    TUD_HID_DESCRIPTOR(ITF_NUM_HID_KBD, 5, HID_ITF_PROTOCOL_KEYBOARD,
        sizeof(desc_hid_report_kbd), EPNUM_HID_KBD, CFG_TUD_HID_EP_BUFSIZE, 5),

    /* Interface 3: Boot Relative Mouse (instance 1) */
    TUD_HID_DESCRIPTOR(ITF_NUM_HID_MOUSE, 6, HID_ITF_PROTOCOL_MOUSE,
        sizeof(desc_hid_report_mouse), EPNUM_HID_MOUSE, CFG_TUD_HID_EP_BUFSIZE, 4),

    /* Interface 4: Consumer Control / Media Keys (instance 2) */
    TUD_HID_DESCRIPTOR(ITF_NUM_HID_CONSUMER, 7, HID_ITF_PROTOCOL_NONE,
        sizeof(desc_hid_report_consumer), EPNUM_HID_CONS, CFG_TUD_HID_EP_BUFSIZE, 10)
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

static char const * const string_desc_arr[] =
{
    (const char[]){ 0x09, 0x04 },
    "DevWithZachary / LNH-team",
    "DSpico Linux Toolkit",
    "DSPICO-0001",
    "DSpico TTY Serial",
    "DSpico HID Keyboard",
    "DSpico HID Touchpad",
    "DSpico HID Media",
};

static uint16_t _desc_str[32 + 1];

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    size_t chr_count;

    if (index == 0)
    {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    }
    else
    {
        if (index >= (sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
        {
            return NULL;
        }
        const char *str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 32)
        {
            chr_count = 32;
        }
        for (size_t i = 0; i < chr_count; i++)
        {
            _desc_str[1 + i] = (uint8_t)str[i];
        }
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
