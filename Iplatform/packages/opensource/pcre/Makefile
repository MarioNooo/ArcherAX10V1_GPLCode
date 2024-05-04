#
# Copyright (C) 2006 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=pcre
PKG_VERSION:=8.30
PKG_RELEASE:=2

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_MD5SUM:=BE0A6D326CD8567E736FBC75DF0A5C45

PKG_FIXUP:=libtool
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk

define Package/pcre
  SECTION:=TP-LINK
  CATEGORY:=TP-LINK iplatform apps
  TITLE:=A lightweight C library that eases the writing of UNIX daemons
  URL:=http://0pointer.de/lennart/projects/libdaemon/
endef

TARGET_CFLAGS += $(CPPFLAGS)

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Build/InstallDev
$(warning ===========$(PKG_INSTALL_DIR)====$(1)=======)
	$(INSTALL_DIR) $(1)/usr/include
	$(CP) $(PKG_INSTALL_DIR)/usr/include/*.h $(1)/usr/include/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libpcre.so* $(1)/usr/lib/
endef

define Package/pcre/install
	$(INSTALL_DIR) $(1)/usr/include
	$(CP) $(PKG_INSTALL_DIR)/usr/include/*.h $(1)/usr/include/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libpcre.so* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,pcre))