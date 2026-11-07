#++
#	FACILITY: A yet another gateway for MODBUS (TCP to RTU) - the OpenWrt feed
#
#	DESCRIPTION: The gateway package for OpenWrt: .ipk by the 24.10 SDK, .apk by the 25.12 one.
#		The sources are the tree this feed lives in (../../), not a download: a build of
#		the feed builds exactly its own commit. The version follows __REV__ of
#		src/t2r_main.c, as the .deb one does (REV 00.13.06 -> 0.13.6).
#
#		libconfig and StarLet are linked statically (../libconfig-static, ../starlet): the
#		package has no dependencies and is installed with no network.
#
#		The service is installed disabled, as in the .deb: the settings file has to be tuned
#		first, then 'uci set mbusgw-t2r.main.enabled=1 && uci commit mbusgw-t2r' and
#		'/etc/init.d/mbusgw-t2r start'.
#
#	AUTHOR: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#	CREATION DATE: 28-OCT-2026
#
#	MODIFICATION HISTORY:
#
#	06-NOV-2026	CLD	V.01-01: the package carries the control utility /usr/sbin/mbus-ctl too.
#--

include $(TOPDIR)/rules.mk

T2R_TOPDIR:=$(abspath $(CURDIR)/../..)

PKG_NAME:=mbusgw-t2r
PKG_VERSION:=$(shell sed -n 's/.*__REV__[[:space:]]*"0*\([0-9][0-9]*\)\.0*\([0-9][0-9]*\)\.0*\([0-9][0-9]*\)".*/\1.\2.\3/p' $(T2R_TOPDIR)/src/t2r_main.c)
PKG_RELEASE:=1

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=LICENSE
PKG_MAINTAINER:=Ruslan R. Laishev <zator@yandex.ru>

PKG_BUILD_DEPENDS:=starlet libconfig-static
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/mbusgw-t2r
  SECTION:=net
  CATEGORY:=Network
  TITLE:=A yet another gateway for MODBUS (TCP to RTU)
  URL:=https://github.com/napilab/mbus-gw-t2r
endef

define Package/mbusgw-t2r/description
 The MODBUS gateway: serves MODBUS TCP clients by the RTU devices on the
 serial lines (RS-232/RS-485, USB adapters). See the User Guide:
 https://github.com/napilab/mbus-gw-t2r/releases
endef

define Package/mbusgw-t2r/conffiles
/etc/config/mbusgw-t2r
/etc/mbusgw-t2r/modbus-t2r.conf
/etc/mbusgw-t2r/modbus-t2r_settings.conf
endef

#	The local tree instead of a download; what CMakeLists.txt installs must be here
define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) $(T2R_TOPDIR)/CMakeLists.txt $(T2R_TOPDIR)/cmake $(T2R_TOPDIR)/src $(T2R_TOPDIR)/contrib \
		$(T2R_TOPDIR)/docs $(T2R_TOPDIR)/*.conf $(T2R_TOPDIR)/*.md $(T2R_TOPDIR)/LICENSE \
		$(T2R_TOPDIR)/NOTICE $(PKG_BUILD_DIR)/
endef

#	The binaries (the gateway, mbus-ctl), the configuration and the procd service only: no docs on a
#	flash of 8..16 MB. mbus-ctl talks to the gateway over /var/run/mbusgw-t2r.sock (the default /CONTROL).
define Package/mbusgw-t2r/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/mbusgw-t2r $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/sbin/mbusgw-t2r $(PKG_INSTALL_DIR)/usr/sbin/mbus-ctl $(1)/usr/sbin/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/modbus-t2r.conf $(PKG_BUILD_DIR)/modbus-t2r_settings.conf $(1)/etc/mbusgw-t2r/
	$(INSTALL_BIN) ./files/mbusgw-t2r.init $(1)/etc/init.d/mbusgw-t2r
	$(INSTALL_CONF) ./files/mbusgw-t2r.config $(1)/etc/config/mbusgw-t2r
endef

$(eval $(call BuildPackage,mbusgw-t2r))
