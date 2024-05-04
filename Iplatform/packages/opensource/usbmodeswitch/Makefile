include $(TOPDIR)/rules.mk

PKG_NAME:=usb-modeswitch
PKG_VERSION:=2.2

PKG_DATA_VERSION:=20140529
PKG_DATA_PATH:=usb-modeswitch-data-$(PKG_DATA_VERSION)

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk


define Package/usb-modeswitch
  SECTION:=TP-LINK
  CATEGORY:=TP-LINK iplatform apps
  DEPENDS:=+libubox +libblobmsg-json +libusb-1.0
  TITLE:=USB mode switching utility
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) $(TARGET_CONFIGURE_OPTS) \
		TARGET_CFLAGS="$(TARGET_CFLAGS)"
endef

define Package/usb-modeswitch/install
	$(CP) ./filesystem/* $(1)/
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/usb_modeswitch $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,usb-modeswitch))
