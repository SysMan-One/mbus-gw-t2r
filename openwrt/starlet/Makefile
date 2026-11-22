#++
#	FACILITY: A yet another gateway for MODBUS (TCP to RTU) - the OpenWrt feed
#
#	DESCRIPTION: The StarLet set (UTILITY$ROUTINES), a static library the gateway is linked
#		with. A build-only package: nothing goes to the device, the library and the
#		find_package(StarLet) files are put into the staging directory of the SDK.
#
#	AUTHOR: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#	CREATION DATE: 28-OCT-2026
#
#	MODIFICATION HISTORY:
#
#	22-NOV-2026	CLD	V.01-03: StarLet 1.8.0 (the tag v1.8.0): yaLIBCFG needs 1.7 at least.
#--

include $(TOPDIR)/rules.mk

PKG_NAME:=starlet
PKG_VERSION:=1.8.0
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://gitlab.com/SysMan-One/utility_routines.git
PKG_SOURCE_VERSION:=a5c15288113f320ec5e1cbc5033265b7fbc25ca3
PKG_MIRROR_HASH:=skip

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=LICENSE
PKG_MAINTAINER:=Ruslan R. Laishev <zator@yandex.ru>

PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_OPTIONS += -DBUILD_TESTING=OFF

define Package/starlet
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=StarLet utility routines (a static library, build only)
  URL:=https://gitlab.com/SysMan-One/utility_routines
  BUILDONLY:=1
endef

define Package/starlet/description
 The StarLet set of the OpenVMS-styled utility routines: messages, logging,
 the command line. A static library: the programs are linked with it, nothing
 is installed on the device.
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr
	$(CP) $(PKG_INSTALL_DIR)/usr/* $(1)/usr/
endef

$(eval $(call BuildPackage,starlet))
