#
# Copyright (C) 2017 Chris Blake <chrisrblake93@gmail.com>
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=leds-apu
PKG_RELEASE:=1

PKG_MAINTAINER:=Chris Blake <chrisrblake93@gmail.com>
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define KernelPackage/leds-apu
  SUBMENU:=LED modules
  TITLE:= PC Engines APU LED support
  DEPENDS:= @GPIO_SUPPORT @TARGET_x86 +kmod-gpio-fch
  FILES:=$(PKG_BUILD_DIR)/leds-apu.ko
  AUTOLOAD:=$(call AutoLoad,30,leds-apu,1)
  KCONFIG:=
endef

define KernelPackage/leds-apu/description
  Driver for the PC Engines APU & Reset Button.
endef

EXTRA_KCONFIG:= \
	CONFIG_LEDS_APU=m

EXTRA_CFLAGS:= \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=m,%,$(filter %=m,$(EXTRA_KCONFIG)))) \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=y,%,$(filter %=y,$(EXTRA_KCONFIG)))) \

MAKE_OPTS:= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	SUBDIRS="$(PKG_BUILD_DIR)" \
	EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
	$(EXTRA_KCONFIG)

define Build/Compile
	 $(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,leds-apu))
