#include <libopencm3/stm32/rcc.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/gpio.h>
#include "usb/msc.h"

#include <string.h>

#include "ramdisk.h"

static const char* usb_strings[] =
{
	"Devanarchy",              //  USB Manufacturer
	"DAPBoot DFU Bootloader",  //  USB Product
	"serial_number",           //  Serial number
	//"Blue Pill DFU",         //  DFU
	"DAPBoot DFU",             //  DFU
	"Blue Pill MSC",           //  MSC
	"Blue Pill Serial Port",   //  Serial Port
	"Blue Pill COMM",          //  COMM
	"Blue Pill DATA",          //  DATA
};

enum usb_strings_index
{
	USB_STRINGS_MANUFACTURER = 1,
	USB_STRINGS_PRODUCT,
	USB_STRINGS_SERIAL_NUMBER,
	USB_STRINGS_DFU,
	USB_STRINGS_MSC,
	USB_STRINGS_SERIAL_PORT,
	USB_STRINGS_COMM,
	USB_STRINGS_DATA,
};

static const struct usb_device_descriptor dev =
{
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0xef, // TODO //USB_CLASS_MISCELLANEOUS,  //  Copied from microbit. For composite device, let host probe the interfaces.
	.bDeviceSubClass = 2,  //  Common Class
	.bDeviceProtocol = 1,  //  Interface Association Descriptor
	.bMaxPacketSize0 = 64,
	.idVendor = 0x1209, // TODO
	.idProduct = 0xdb42, // TODO
	.bcdDevice = 0x0220,  //  Device Release number 2.2 // TODO
	.iManufacturer = USB_STRINGS_MANUFACTURER,
	.iProduct = USB_STRINGS_PRODUCT,
	.iSerialNumber = USB_STRINGS_SERIAL_NUMBER,
	.bNumConfigurations = 1,
};

static const struct usb_endpoint_descriptor msc_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x01, // TODO
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 0,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x82, // TODO
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 0,
}};

static const struct usb_interface_descriptor msc_iface =
{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1, // TODO -> 0
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_MSC,
	.bInterfaceSubClass = USB_MSC_SUBCLASS_SCSI,
	.bInterfaceProtocol = USB_MSC_PROTOCOL_BBB,
	.iInterface = USB_STRINGS_MSC,  //  Name of MSC
	.endpoint = msc_endp,  //  MSC Endpoints
	.extra = NULL, // TODO
	.extralen = 0
};

static const struct usb_interface interfaces[] = {
	{
		.num_altsetting = 1,
		.altsetting = &msc_iface,  //  Index must sync with INTF_MSC.
	}, 	
};

static const struct usb_config_descriptor config =
{
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = sizeof(interfaces) / sizeof(struct usb_interface),
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,  //  Bus-powered, i.e. it draws power from USB bus.
	.bMaxPower = 0xfa,     //  500 mA. Copied from microbit.
	.interface = interfaces,
};

static uint8_t usbd_control_buffer[256] __attribute__ ((aligned (2)));

int main(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	rcc_periph_clock_enable(RCC_GPIOA); // for usb (PA11, PA12)
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_USB); // TODO: not required

	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

	/* Drive the USB DP pin to override the pull-up */ // TODO
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
	rcc_periph_reset_pulse(RST_USB);
	/* Override hard-wired USB pullup to disconnect and reconnect */
	gpio_clear(GPIOA, GPIO12);
	for (int i = 0; i < 800000; i++)
		__asm__("nop");

	int num_strings = sizeof(usb_strings) / sizeof(const char*);
	usbd_device* usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev, &config, 
		usb_strings, num_strings,
		usbd_control_buffer, sizeof(usbd_control_buffer));

	ramdisk_init();

	usb_msc_init(usbd_dev, 0x82, 64, 0x01, 64, 
		"BluePill", "UF2 Bootloader", "2.1", 
		ramdisk_blocks(), ramdisk_read, ramdisk_write
	);
	 
	while (1)
	{
		usbd_poll(usbd_dev);
	}

	return 0;
}
