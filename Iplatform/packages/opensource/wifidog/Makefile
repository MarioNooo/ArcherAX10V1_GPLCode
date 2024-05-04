#
# Copyright (C) 2010-2013 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=wifidog

PKG_VERSION:=1.1.1
PKG_RELEASE:=1

# PKG_SOURCE_URL:=http://swupdate.openvpn.net/community/releases

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

PKG_INSTALL:=1
PKG_FIXUP:=autoreconf
PKG_BUILD_PARALLEL:=1
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  TITLE:=Open source wifidog solution using $(2)
#  SECTION:=net
  CATEGORY:=TP-LINK iplatform apps
  URL:=http://wifidog.net
#  SUBMENU:=wifidog
  MENU:=1
endef

define Build/Configure
	$(call Build/Configure/Default)
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/include
	$(INSTALL_DIR) $(1)/etc/wifidog
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/etc/wifidog/locale

	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/wifidog $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/wdctl $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/lib/* $(1)/usr/lib/
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/include/wifidog/* $(1)/usr/include/
	
	$(CP) filesystem/etc/wifidog/* $(1)/etc/wifidog/
	$(INSTALL_BIN) filesystem/etc/init.d/wifidog $(1)/etc/init.d/wifidog
endef

$(eval $(call BuildPackage,$(PKG_NAME)))

