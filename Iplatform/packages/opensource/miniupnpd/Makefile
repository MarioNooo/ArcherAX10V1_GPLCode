#
# Copyright (C) 2006-2013 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=miniupnpd
PKG_VERSION:=1.8.20130426
PKG_RELEASE:=2

PKG_SOURCE_URL:=http://miniupnp.free.fr/files
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_MD5SUM:=ca5344eba200e4a6d1313849d68310f6

include $(INCLUDE_DIR)/package.mk

define Package/miniupnpd
  SECTION:=TP-LINK
  CATEGORY:=TP-LINK iplatform apps
  DEPENDS:=+iptables +libip4tc +IPV6:libip6tc +libnfnetlink +libifaddrs
  TITLE:=Lightweight UPnP daemon
  URL:=http://miniupnp.free.fr/
  MAINTAINER:=Daniel Dickinson <openwrt@cshore.neomailbox.net>
endef

define Package/miniupnpd/conffiles
/etc/config/upnpd
endef

define Package/miniupnpd/postinst
#!/bin/sh

if [ -z "$$IPKG_INSTROOT" ]; then
  ( . /etc/uci-defaults/99-miniupnpd )
  rm -f /etc/uci-defaults/99-miniupnpd
fi

exit 0
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -u ./src/* $(PKG_BUILD_DIR)/
	if [ "$(CONFIG_WEBPAGES_TPLINK_TPLink)" = "y" ]; then \
		echo "TP-Link" | tr \(\)\  _ >$(PKG_BUILD_DIR)/os.tplink; \
	else \
		echo "TP-LINK" | tr \(\)\  _ >$(PKG_BUILD_DIR)/os.tplink; \
	fi
endef

MAKE_FLAGS += \
	LIBS="" \
	CC="$(TARGET_CC) -DIPTABLES_143 -lnfnetlink -lifaddrs -lip4tc $(if $(CONFIG_IPV6),-lip6tc)" \
	CONFIG_OPTIONS="$(if $(CONFIG_IPV6),--ipv6) --leasefile" \
	-f Makefile.linux \
	miniupnpd

define Package/miniupnpd/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config $(1)/etc/uci-defaults $(1)/etc/hotplug.d/iface $(1)/usr/share/miniupnpd
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/miniupnpd $(1)/usr/sbin/miniupnpd
	$(INSTALL_BIN) ./filesystem/miniupnpd.init $(1)/etc/init.d/miniupnpd
	$(INSTALL_CONF) ./filesystem/upnpd.config $(1)/etc/config/upnpd
	$(INSTALL_DATA) ./filesystem/miniupnpd.hotplug $(1)/etc/hotplug.d/iface/50-miniupnpd
	$(INSTALL_DATA) ./filesystem/miniupnpd.defaults $(1)/etc/uci-defaults/99-miniupnpd
	$(INSTALL_DATA) ./filesystem/firewall.include $(1)/usr/share/miniupnpd/firewall.include
endef

$(eval $(call BuildPackage,miniupnpd))
