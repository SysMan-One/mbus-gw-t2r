#++
#	FACILITY: A yet another gateway for MODBUS (TCP to RTU) - the OpenWrt feed
#
#	DESCRIPTION: yaLIBCFG, a static library the gateway reads its settings file with (the
#		libconfig syntax, a schema, the positional diagnostics). A build-only package: nothing
#		goes to the device, the library and the find_package(yaLIBCFG) files are put into the
#		staging directory of the SDK. It is built on StarLet (../starlet).
#
#	AUTHOR: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#	CREATION DATE: 22-NOV-2026
#
#	MODIFICATION HISTORY:
#
#	22-NOV-2026	CLD	V.01-03: Initial revision (the release X.01-01), replaces libconfig-static.
#--

include $(TOPDIR)/rules.mk

PKG_NAME:=yalibcfg
PKG_VERSION:=1.1.0
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/SysMan-One/yaLIBCFG.git
PKG_SOURCE_VERSION:=d2a9aa698ea41831ebb85c2e180412a9eea58020
PKG_MIRROR_HASH:=skip

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=LICENSE
PKG_MAINTAINER:=Ruslan R. Laishev <zator@yandex.ru>

PKG_BUILD_DEPENDS:=starlet
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_OPTIONS += -DYALIBCFG_M_HELPLIB=OFF -DYALIBCFG_M_EXAMPLES=OFF -DYALIBCFG_M_TESTS=OFF

define Package/yalibcfg
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=yaLIBCFG configuration trees (a static library, build only)
  URL:=https://github.com/SysMan-One/yaLIBCFG
  BUILDONLY:=1
endef

define Package/yalibcfg/description
 yaLIBCFG: configuration files in the libconfig syntax, with a schema
 validation and the positional diagnostics. A static library: the programs
 are linked with it, nothing is installed on the device.
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr
	$(CP) $(PKG_INSTALL_DIR)/usr/* $(1)/usr/
endef

$(eval $(call BuildPackage,yalibcfg))
