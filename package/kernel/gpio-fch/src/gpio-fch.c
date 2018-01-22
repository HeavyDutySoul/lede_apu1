/*
 * gpio-fch.c - GPIO interface for AMD Fusion Controller Hub
 *
 * Copyright (C) 2013 CompuLab Ltd 
 * Denis Turischev <denis.turischev@compulab.co.il>
 *
 * Copyright (C) 2014 iGetech Innova, S.L.
 * Jordi Ferrer Plana <jferrer@igetech.com>
 *
 * NOTES:
 * 	1. We assume there can only be one FCH PCI device in the system.
 * 	2. Tested on PC Engines APU Board.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/gpio.h>
#include <linux/i2c.h>

/* Module Information */
#define FCH_MODULE_NAME			"gpio-fch"
#define FCH_MODULE_VER			"0.1"
#define FCH_DRIVER_NAME			(FCH_MODULE_NAME " (v" FCH_MODULE_VER ")")

/* For SB8x0(or later) chipset */
#define SB800_IO_PM_INDEX_REG           0xcd6
#define SB800_IO_PM_DATA_REG            0xcd7
#define SB800_IO_PM_SIZE		(SB800_IO_PM_DATA_REG-SB800_IO_PM_INDEX_REG+1)
#define SB800_PM_ACPI_MMIO_EN		0x24	/* SMBus offset for FCH MMIO base addr */

#define FCH_GPIO_SPACE_OFFSET		0x100	/* GPIO offset from FCH base MMIO ADDR */
#define FCH_GPIO_SPACE_SIZE		0x100	/* GPIO MMIO space size */

/* Internal Variables */
static void __iomem *gpio_ba;
static u32 acpimmioaddr;
static DEFINE_SPINLOCK(gpio_lock);

static DEFINE_PCI_DEVICE_TABLE(gpio_fch_tbl) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_SBX00_SMBUS) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, gpio_fch_tbl);

/* Read SBResource_MMIO from AcpiMmioEn(PM_Reg: 24h) */
u32 read_pm_reg(u8 addr)
{
	u32 res;

	outb(addr + 3, SB800_IO_PM_INDEX_REG);
	res = inb(SB800_IO_PM_DATA_REG);
	res = res << 8;

	outb(addr + 2, SB800_IO_PM_INDEX_REG);
	res = res + inb(SB800_IO_PM_DATA_REG);
	res = res << 8;

	outb(addr + 1, SB800_IO_PM_INDEX_REG);
	res = res + inb(SB800_IO_PM_DATA_REG);
	res = res << 8;

	/* Lower byte not needed */

	return res;
}

static int gpio_fch_direction_in(struct gpio_chip *gc, unsigned gpio_num)
{
	u8 curr_state;

	/* Compute absolute gpio addr */
	gpio_num += gc->base;

	spin_lock(&gpio_lock);

	curr_state = ioread8(gpio_ba + gpio_num);
	if (!(curr_state & BIT(5)))
		iowrite8(curr_state | BIT(5), gpio_ba + gpio_num);

	spin_unlock(&gpio_lock);

	return 0;
}

static int gpio_fch_direction_out(struct gpio_chip *gc, unsigned gpio_num, int val)
{
	u8 curr_state;

	/* Compute absolute gpio addr */
	gpio_num += gc->base;

	spin_lock(&gpio_lock);

	curr_state = ioread8(gpio_ba + gpio_num);
	if (curr_state & BIT(5))
        	iowrite8(curr_state & ~BIT(5), gpio_ba + gpio_num);

	spin_unlock(&gpio_lock);

	return 0;
}

static int gpio_fch_get(struct gpio_chip *gc, unsigned gpio_num)
{
	u8 curr_state, bit;
	int res;

	/* Compute absolute gpio addr */
	gpio_num += gc->base;

	curr_state = ioread8(gpio_ba + gpio_num);
	bit = (curr_state & BIT(5)) ? 7 : 6;
	res = !!(curr_state & BIT(bit));

	return res;
}

static void gpio_fch_set(struct gpio_chip *gc, unsigned gpio_num, int val)
{
	u8 curr_state;

	/* Compute absolute gpio addr */
	gpio_num += gc->base;

	spin_lock(&gpio_lock);

	/* Set to "out" in case it is not */
	curr_state = ioread8(gpio_ba + gpio_num);
	if (curr_state & BIT(5)) {
		curr_state &= ~BIT(5);
		iowrite8(curr_state, gpio_ba + gpio_num);
	}

	if (val)
		iowrite8(curr_state | BIT(6), gpio_ba + gpio_num);
	else
		iowrite8(curr_state & ~BIT(6), gpio_ba + gpio_num);

	spin_unlock(&gpio_lock);
}

static struct gpio_chip fch_gpio_chip0 = {
	.label = FCH_MODULE_NAME,
	.owner = THIS_MODULE,
	.get = gpio_fch_get,
	.direction_input = gpio_fch_direction_in,
	.set = gpio_fch_set,
	.direction_output = gpio_fch_direction_out,
	.base = 0,
	.ngpio = 68,
};

static struct gpio_chip fch_gpio_chip128 = {
	.label = FCH_MODULE_NAME,
	.owner = THIS_MODULE,
	.get = gpio_fch_get,
	.direction_input = gpio_fch_direction_in,
	.set = gpio_fch_set,
	.direction_output = gpio_fch_direction_out,
	.base = 128,
	.ngpio = 23,
};

static struct gpio_chip fch_gpio_chip160 = {
	.label = FCH_MODULE_NAME,
	.owner = THIS_MODULE,
	.get = gpio_fch_get,
	.direction_input = gpio_fch_direction_in,
	.set = gpio_fch_set,
	.direction_output = gpio_fch_direction_out,
	.base = 160,
	.ngpio = 69,
};

static int gpio_fch_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int ret;
  
	/* Request the I/O ports used by this driver */
	if (!request_region(SB800_IO_PM_INDEX_REG, SB800_IO_PM_SIZE, FCH_MODULE_NAME)) {
		dev_err(&pdev->dev, "SMBus base address index region "
			"[0x%x .. 0x%x] already in use!\n",
			SB800_IO_PM_INDEX_REG, SB800_IO_PM_DATA_REG);
		return -EBUSY;
	}
 
	/* Read SBResource_MMIO from AcpiMmioEn(PM_Reg: 24h) */
	acpimmioaddr = read_pm_reg(SB800_PM_ACPI_MMIO_EN) & 0xFFFFFF00;

	/* Release I/O Ports */
	release_region(SB800_IO_PM_INDEX_REG, SB800_IO_PM_SIZE);

	dev_info(&pdev->dev, "Loading driver %s. FCH GPIO MMIO Range [0x%8x,0x%8x]\n",
			FCH_DRIVER_NAME, acpimmioaddr+FCH_GPIO_SPACE_OFFSET,
			acpimmioaddr+FCH_GPIO_SPACE_OFFSET+FCH_GPIO_SPACE_SIZE);

	if ( !request_mem_region(acpimmioaddr + FCH_GPIO_SPACE_OFFSET,
				FCH_GPIO_SPACE_SIZE, fch_gpio_chip0.label) )
		return -EBUSY;

	gpio_ba = ioremap (acpimmioaddr + FCH_GPIO_SPACE_OFFSET, FCH_GPIO_SPACE_SIZE);
	if ( !gpio_ba ) {
		ret = -ENOMEM;
		goto err_no_ioremap;
	}

	ret = gpiochip_add(&fch_gpio_chip0);
	if (ret < 0)
		goto err_no_gpiochip0_add;

	ret = gpiochip_add(&fch_gpio_chip128);
	if (ret < 0)
		goto err_no_gpiochip128_add;

	ret = gpiochip_add(&fch_gpio_chip160);
	if (ret < 0)
		goto err_no_gpiochip160_add;

	return 0;

err_no_gpiochip160_add:
	/*ret = */ gpiochip_remove(&fch_gpio_chip128);
	//if (ret)
	//	dev_err(&pdev->dev, "%s failed, %d", "gpiochip_remove()", ret);

err_no_gpiochip128_add:
	/*ret = */ gpiochip_remove(&fch_gpio_chip0);
	//if (ret)
	//	dev_err(&pdev->dev, "%s failed, %d", "gpiochip_remove()", ret);

err_no_gpiochip0_add:
	iounmap(gpio_ba);

err_no_ioremap:
	release_mem_region(acpimmioaddr + FCH_GPIO_SPACE_OFFSET, FCH_GPIO_SPACE_SIZE);
	return ret;
}

static void gpio_fch_remove(struct pci_dev *pdev)
{
	int ret;

	dev_info(&pdev->dev, "Unloading driver %s (PCI 0x%4x:0x%4x)\n",
		FCH_DRIVER_NAME, pdev->vendor, pdev->device );

	/*ret = */ gpiochip_remove(&fch_gpio_chip160);
	//if (ret < 0)
	//	dev_err(&pdev->dev, "%s failed, %d", "gpiochip_remove()", ret);

	/*ret = */ gpiochip_remove(&fch_gpio_chip128);
	//if (ret < 0)
	//	dev_err(&pdev->dev, "%s failed, %d", "gpiochip_remove()", ret);

	/*ret = */ gpiochip_remove(&fch_gpio_chip0);
	//if (ret < 0)
	//	dev_err(&pdev->dev, "%s failed, %d", "gpiochip_remove()", ret);

	iounmap(gpio_ba);

	release_mem_region(acpimmioaddr + FCH_GPIO_SPACE_OFFSET, FCH_GPIO_SPACE_SIZE);
}

static struct pci_driver gpio_fch_driver = {
	.name = FCH_MODULE_NAME,
	.id_table = gpio_fch_tbl,
	.probe = gpio_fch_probe,
	.remove = gpio_fch_remove,
};

module_pci_driver(gpio_fch_driver);

MODULE_AUTHOR("Denis Turischev <denis@compulab.co.il> & Jordi Ferrer Plana <jferrer@igetech.com>");
MODULE_DESCRIPTION("GPIO interface for AMD Fusion Controller Hub");
MODULE_LICENSE("GPL");
