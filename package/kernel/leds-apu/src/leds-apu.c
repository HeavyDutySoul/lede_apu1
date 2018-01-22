/*
 * Set up PC Engines APU board front LEDs using GPIOLIB and GPIO-FCH
 * drivers. This driver is based on the geode/alix.c.
 *
 * Copyright (C) 2014 Jordi Ferrer Plana <jferrer@igetech.com>
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING. If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/dmi.h>

/* Module Information */
#define APU_MODULE_NAME			"leds-apu"
#define APU_MODULE_VER			"0.1"
#define APU_DRIVER_NAME			(APU_MODULE_NAME " (v" APU_MODULE_VER ")")

static struct gpio_keys_button apu_gpio_buttons[] = {
	{
		.code			= KEY_RESTART,
		.gpio			= 187,
		.active_low		= 1,
		.desc			= "Reset button",
		.type			= EV_KEY,
		.wakeup			= 0,
		.debounce_interval	= 100,
		.can_disable		= 0,
	}
};
static struct gpio_keys_platform_data apu_buttons_data = {
	.buttons			= apu_gpio_buttons,
	.nbuttons			= ARRAY_SIZE(apu_gpio_buttons),
	.poll_interval			= 20,
};

static struct platform_device apu_buttons_dev = {
	.name				= "gpio-keys-polled",
	.id				= 1,
	.dev = {
		.platform_data		= &apu_buttons_data,
	}
};

static struct gpio_led apu_leds[] = {
	{
		.name = "apu:1",
		.gpio = 189,
		.default_trigger = "default-on",
		.active_low = 1,
	},
	{
		.name = "apu:2",
		.gpio = 190,
		.default_trigger = "default-off",
		.active_low = 1,
	},
	{
		.name = "apu:3",
		.gpio = 191,
		.default_trigger = "default-off",
		.active_low = 1,
	},
};

static struct gpio_led_platform_data apu_leds_data = {
	.num_leds = ARRAY_SIZE(apu_leds),
	.leds = apu_leds,
};

static struct platform_device apu_leds_dev = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &apu_leds_data,
};

static struct __initdata platform_device *apu_devs[] = {
	&apu_buttons_dev,
	&apu_leds_dev,
};

static void __init register_apu(void)
{
	/* Setup LED control through leds-gpio driver */
	platform_add_devices(apu_devs, ARRAY_SIZE(apu_devs));
}

static bool __init apu_present_dmi(void)
{
	const char *vendor, *product;

	vendor = dmi_get_system_info(DMI_SYS_VENDOR);
	if (!vendor || strcmp(vendor, "PC Engines"))
		return false;

	product = dmi_get_system_info(DMI_PRODUCT_NAME);
	if (!product || (strcmp(product, "APU1")))
		return false;

	printk(KERN_INFO "%s: System is recognized as \"%s %s\"\n",
		APU_DRIVER_NAME, vendor, product);

	return true;
}

static int __init apu_init(void)
{
	/* Check for APU board */
	printk(KERN_INFO "Loading driver %s.\n", APU_DRIVER_NAME);
	if (apu_present_dmi())
		register_apu();

	return 0;
}

module_init(apu_init);

MODULE_AUTHOR("Jordi Ferrer Plana <jferrer@igetech.com>");
MODULE_DESCRIPTION("PC Engines APU GPIO-based LED Driver");
MODULE_LICENSE("GPL");
