#
# Basic targets for special product
#
SOFTWARE_VERSION?="V1.0.0P1"

sdk_post:
	[ ! -d "$(SDK_KERNEL_DIR)/modules" ] || rm -rf $(SDK_KERNEL_DIR)/modules
	mkdir -p $(SDK_KERNEL_DIR)/modules
	for node in `find $(SDK_KERNEL_DIR) -name "*.ko" -type f`; do \
		cp -f $$node $(SDK_KERNEL_DIR)/modules; \
	done
	
kernel:
	#@rm -rf $(SDK_BUILD_DIR)/.last_profile; \
	$(MAKE) -C $(SDK_BUILD_DIR) PROFILE=TP6750 kernelbuild buildimage

kernel.config:
	@echo "copy kernel.config"
	#@cp -f $(CONFIG_DIR)/kernel.config $(SDK_KERNEL_DIR)/.config

sdk_all:
	#@rm -rf $(SDK_BUILD_DIR)/.last_profile
	$(MAKE) -C $(SDK_BUILD_DIR) PROFILE=TP6750

sdk: sdk_all sdk_post
sdk_menuconfig: menuconfig 

kernel_menuconfig:
	cd $(SDK_KERNEL_DIR) && make menuconfig
