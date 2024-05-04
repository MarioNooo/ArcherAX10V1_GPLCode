#
# Generic Makefile to build images for various type of products.
# Author: TengFei
# Date: Aug 13th, 2014.
#

PRODUCT_NAME=ax10v1.20

# our make tool
#
SUBMAKE:=$(MAKE)

#
# path name of common dirs 
#
BUILD_DIR:=$(PWD)
CONFIG_DIR:=$(BUILD_DIR)/product_configs/$(PRODUCT_NAME)
PLATFORM_DIR:=$(BUILD_DIR)/../openwrt
IMAGE_DIR:=$(BUILD_DIR)/../image/$(PRODUCT_NAME)

#
# basic Vars for special product
#
include $(CONFIG_DIR)/basic.config

# default target
platform:

#
# basic targets for special product
#
include $(CONFIG_DIR)/product.mk

export CONFIG_PRODUCT_NAME=$(PRODUCT_NAME)

platform: iplatform_prep sdk iplatform

toolchain: 
	@if [ ! -d $(TOOLCHAIN_ROOT_DIR) ]; then \
		echo Uncompressing toolchain, please wait... && \
		tar -jxf $(TOOLCHAIN_DIR)/$(TOOLCHAIN_PACKAGE) -C $(TOOLCHAIN_DIR) ; \
	 fi
	@echo make $@ done.

iplatform.config:
	@cp -f $(CONFIG_DIR)/iplatform.config $(PLATFORM_DIR)/.config

iplatform_prep: feed.conf toolchain iplatform.config iplatform_package/symlinks kernel.config $(EXT_PREPARE)

feed.conf:
	@if [ -e $(BUILD_DIR)/feeds.conf ]; then \
		cp -f $(BUILD_DIR)/feeds.conf $(PLATFORM_DIR)/feeds.conf; \
	fi

iplatform: iplatform_world

%::
	$(if $(filter iplatform_%,$@),\
		@$(SUBMAKE) -C $(PLATFORM_DIR) $(patsubst iplatform_%, %, $@),\
		@$(SUBMAKE) -C $(SDK_BUILD_DIR) $@ $(SDK_FLAGS))

.PHONY: toolchain iplatform_prep iplatform platform sdk \
	iplatform.config kernel.config  $(EXT_PHONY)
