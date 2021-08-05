#include <libopencm3/stm32/rcc.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/gpio.h>
#include "usb/msc.h"

#include <string.h>

#include "ramdisk.h"

#define MAX_PACKET_SIZE 64

#define MSC_ENDPOINT_IN  0x82
#define MSC_ENDPOINT_OUT 0x01

static const char* usb_strings[] =
{
	"Devanarchy",              //  USB Manufacturer
	"DAPBoot DFU Bootloader",  //  USB Product
	"serial_number",           //  Serial number
	"Blue Pill MSC",           //  MSC
};

enum usb_strings_index
{
	MANUFACTURER = 1,
	PRODUCT,
	SERIAL_NUMBER,
	MSC_NAME,
};

static const struct usb_device_descriptor device =
{
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = MAX_PACKET_SIZE,
	.idVendor = 0x0483, // STMicroelectronics
	.idProduct = 0x5720, // Mass Storage Device
	.bcdDevice = 0x0200,
	.iManufacturer = MANUFACTURER,
	.iProduct = PRODUCT,
	.iSerialNumber = SERIAL_NUMBER,
	.bNumConfigurations = 1,
};

static const struct usb_endpoint_descriptor msc_endpoints[] =
{
	{
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = MSC_ENDPOINT_OUT,
		.bmAttributes = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize = MAX_PACKET_SIZE,
		.bInterval = 0,
		.extra = NULL,
		.extralen = 0,
	},
	{
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = MSC_ENDPOINT_IN,
		.bmAttributes = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize = MAX_PACKET_SIZE,
		.bInterval = 0,
		.extra = NULL,
		.extralen = 0,
	}
};

static const struct usb_interface_descriptor msc_interface =
{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_MSC,
	.bInterfaceSubClass = USB_MSC_SUBCLASS_SCSI,
	.bInterfaceProtocol = USB_MSC_PROTOCOL_BBB,
	.iInterface = MSC_NAME,
	.endpoint = msc_endpoints,
	.extra = NULL,
	.extralen = 0
};

static const struct usb_interface usb_interfaces[] =
{
	{
		.cur_altsetting = 0,
		.num_altsetting = 1,
		.iface_assoc = NULL,
		.altsetting = &msc_interface,
	}, 	
};

static const struct usb_config_descriptor config =
{
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = sizeof(usb_interfaces) / sizeof(struct usb_interface),
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32, // 100mA
	.interface = usb_interfaces,
};

static uint8_t usbd_control_buffer[128];

int main(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	rcc_periph_clock_enable(RCC_GPIOA); // for usb (PA11, PA12)

	usbd_device* usbd_dev = usbd_init(
		&st_usbfs_v1_usb_driver, &device, &config,
		usb_strings, sizeof(usb_strings) / sizeof(char*),
		usbd_control_buffer, sizeof(usbd_control_buffer)
	);

	ramdisk_init();

	usb_msc_init(
		usbd_dev, 0x82, 64, 0x01, 64, 
		"BluePill", "UF2 Bootloader", "2.1", 
		ramdisk_blocks(), ramdisk_read, ramdisk_write
	);
	 
	while (1)
	{
		usbd_poll(usbd_dev);
	}

	return 0;
}
