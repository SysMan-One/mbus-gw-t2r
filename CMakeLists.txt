#++
#	FACILITY: A yet another gateway for MODBUS (TCP to RTU)
#
#	AUTHOR: StarLet Squad and Ruslan R. Laishev (AKA: BadAss sysman)
#
#	CREATION DATE: 10-SEP-2025
#
#	MODIFICATION HISTORY:
#
#	25-AUG-2026	RRL	X.00-04: Diagnostic options are ON for any build type;
#				added ASAN build type;
#				fixed CMAKE_HOST_SYSTEM_PROCESSOR -> CMAKE_SYSTEM_PROCESSOR (cross-building);
#				fixed order of cmake_minimum_required()/project().
#
#	25-AUG-2026	RRL	X.00-06ECO01: The User Guides are delivered as the DEC/VSI-styled PDFs too
#				(docs/UserGuide_EN.pdf, docs/UserGuide_RU.pdf, the generator is
#				docs/make_userguide_pdf.py); both are installed with the docs.
#
#	25-AUG-2026	RRL	X.00-08: The musl C library builds are supported: added the toolchain file
#				cmake/musl.cmake (the kernel headers are appended by -idirafter, so the
#				musl ones always win); the library directory reported by pkg-config is
#				given to the linker explicitly - a libconfig installed to a non-standard
#				prefix was not found at the link time.
#
#	25-AUG-2026	RRL	X.00-07ECO04: README is delivered in two languages (README.md +
#				README_RU.md), both are installed with the docs.
#
#	25-AUG-2026	RRL	X.00-06: The bilingual (EN + RU) User Guide is installed with the docs.
#
#	25-AUG-2026	RRL	X.00-05ECO01: Removed the obsolete modbus-t2r_startup.sh (referenced the
#				pre-package layout); a service management is a matter of a systemd unit
#				of the target system.
#
#	25-AUG-2026	RRL	X.00-05: The 3rdparty/ submodules are gone - the StarLet set is consumed
#				as an installed package now: find_package(StarLet) + StarLet::starlet;
#				added the <install> and <uninstall> targets: the binary goes to sbin/,
#				the configuration examples - to etc/mbusgw-t2r/ (the existing files
#				are never overwritten), the docs - to share/doc/mbusgw-t2r/.
#
#	14-OCT-2026	RRL	X.00-09: The default installation prefix is /opt/mbusgw-t2r (a
#				self-contained tree: sbin/, etc/, share/doc/mbusgw-t2r/); an explicit
#				-DCMAKE_INSTALL_PREFIX=... still works as before. Under /opt/ the
#				configuration goes to <prefix>/etc/ - not to the GNUInstallDirs'
#				/etc/opt/<prefix>/. Fixed the doubled DESTDIR in the configuration
#				files path (file (INSTALL) prepends DESTDIR by itself).
#
#	14-OCT-2026	RRL	X.00-10: contrib/99-ftdi-latency.rules (the FTDI latency_timer = 1 msec udev
#				rule) is installed with the docs.
#
#	14-OCT-2026	RRL	X.00-12ECO03: The package has a version: project(... VERSION) is taken from
#				__REV__ of src/t2r_main.c (REV 00.12.03 -> VERSION 0.12.3).
#
#	28-OCT-2026	RRL	X.00-13ECO04: libconfig is linked statically by default (T2R_STATIC_LIBCONFIG):
#				one package runs on any distro, whatever its libconfig.so.N (noble has
#				libconfig9, trixie - libconfig11); -DT2R_STATIC_LIBCONFIG=OFF links the
#				shared one as before.
#
#	28-OCT-2026	RRL	X.00-13ECO05: The .deb package by CPack (cpack -G DEB): the systemd unit
#				contrib/mbusgw-t2r.service (installed disabled, T2R_SYSTEMD_UNIT), the
#				configuration files are conffiles; built by .github/workflows/release.yml.
#
#	28-OCT-2026	RRL	X.00-13ECO06: The .deb of a cross build: T2R_DEB_ARCH names the Debian
#				architecture of the package (mipsel, mips64el) - CPack takes the one of
#				the build host otherwise. The OpenWrt packages are built from openwrt/.
#
#	06-NOV-2026	CLD	V.01-01: The control utility mbus-ctl (src/mbus_ctl.c) is built and installed
#				next to the gateway (sbin/), the gateway got the module src/t2r_ctl.c (the
#				control channel); the .deb gets mbus-ctl by the same install() rule.
#				The package (T2R_SYSTEMD_UNIT) adds the symlink /usr/sbin/mbus-ctl, so that the
#				utility is in PATH.
#
#	USAGE (see README.md for the whole installation sequence):
#		$ mkdir ./build && cd ./build
#		$ cmake ../ -DCMAKE_BUILD_TYPE=Release
#		$ make -s
#		$ sudo make install
#		$ sudo make uninstall
#
#	The .deb package (see .github/workflows/release.yml):
#		$ cmake ../ -DCMAKE_BUILD_TYPE=Release -DT2R_SYSTEMD_UNIT=ON
#		$ make -s && cpack -G DEB
#
#	The OpenWrt packages (.ipk, .apk): the feed openwrt/, see README.md.
#--

cmake_minimum_required (VERSION 3.10)
cmake_policy(SET CMP0048 NEW)

include(CMakePrintHelpers)
include(CheckSymbolExists)



#
#   The package version is taken from __REV__ of the head module (src/t2r_main.c): the IDENT/REV
#   pair is the single source of the revision, X.00-12ECO03 -> REV 00.12.03 -> VERSION 0.12.3.
#
file (STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/t2r_main.c" T2R_REV_LINE REGEX "^#define[ \t]+__REV__[ \t]+\"[0-9.]+\"")
string (REGEX REPLACE ".*\"([0-9]+)\.([0-9]+)\.([0-9]+)\".*" "\\1;\\2;\\3" T2R_REV_LIST "${T2R_REV_LINE}")
list (LENGTH T2R_REV_LIST T2R_REV_N)
if (NOT T2R_REV_N EQUAL 3)
	message (FATAL_ERROR "Cannot get __REV__ from src/t2r_main.c: <${T2R_REV_LINE}>")
endif ()
list (GET T2R_REV_LIST 0 T2R_REV_MAJOR)
list (GET T2R_REV_LIST 1 T2R_REV_MINOR)
list (GET T2R_REV_LIST 2 T2R_REV_ECO)
math (EXPR T2R_REV_MAJOR "${T2R_REV_MAJOR}")				# "00" -> "0"
math (EXPR T2R_REV_MINOR "${T2R_REV_MINOR}")
math (EXPR T2R_REV_ECO "${T2R_REV_ECO}")

project(mbusgw-t2r VERSION ${T2R_REV_MAJOR}.${T2R_REV_MINOR}.${T2R_REV_ECO} LANGUAGES C)
message (STATUS "mbusgw-t2r package version: ${PROJECT_VERSION}")

#
#   The default installation prefix is /opt/mbusgw-t2r; must precede GNUInstallDirs, which
#   derives the installation directories from it. An explicit -DCMAKE_INSTALL_PREFIX wins.
#
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set (CMAKE_INSTALL_PREFIX "/opt/${PROJECT_NAME}" CACHE PATH "Installation prefix" FORCE)
endif ()

include(GNUInstallDirs)							# Must follow project(): needs the compiler info
set ( CMAKE_VERBOSE_MAKEFILE ON )
set ( CMAKE_COLOR_MAKEFILE   ON )

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCONFIG REQUIRED libconfig)

#
#   LIBCONFIG_LIBRARIES holds the bare "-lconfig" name, so the library directory reported by
#   pkg-config must be given to the linker explicitly - otherwise a libconfig installed to a
#   non-standard prefix (a musl sysroot, /opt/...) is not found at the link time.
#
link_directories(${LIBCONFIG_LIBRARY_DIRS})

#
#   libconfig is linked statically by default: the soname differs from distro to distro (libconfig9,
#   libconfig11, ...), so a binary linked with the shared one runs only where it was built. The
#   library has no dependencies beyond libc; LGPL-2.1 is satisfied by the open sources (see NOTICE).
#
option(T2R_STATIC_LIBCONFIG "Link libconfig statically" ON)

if (T2R_STATIC_LIBCONFIG)
	find_library(LIBCONFIG_STATIC_LIB NAMES libconfig.a HINTS ${LIBCONFIG_LIBRARY_DIRS} NO_CMAKE_SYSTEM_PATH)
	if (NOT LIBCONFIG_STATIC_LIB)
		find_library(LIBCONFIG_STATIC_LIB NAMES libconfig.a)
	endif ()
	if (NOT LIBCONFIG_STATIC_LIB)
		message (FATAL_ERROR "libconfig.a is not found (install libconfig-dev) - or give -DT2R_STATIC_LIBCONFIG=OFF")
	endif ()
	set (T2R_LIBCONFIG_LINK ${LIBCONFIG_STATIC_LIB})
else ()
	set (T2R_LIBCONFIG_LINK ${LIBCONFIG_LIBRARIES})
endif ()
message (STATUS "libconfig: ${T2R_LIBCONFIG_LINK}")


find_package(StarLet REQUIRED)						# The installed UTILITY$ROUTINES package,
									# see README.md how to install it


set ( T2R_TARGET        "mbusgw-t2r" )

add_definitions( -D__ARCH__NAME__="${CMAKE_SYSTEM_PROCESSOR}" )		# NOT ..._HOST_..., see cross-building
add_definitions( -D_REENTRANT=1 )
cmake_print_variables(CMAKE_CURRENT_SOURCE_DIR)


add_compile_options(							# Diagnostic is ON for any build type
	-Wall -Wextra -Wformat=2
	-Werror=format
	-Werror=return-type
	-Werror=implicit-function-declaration
	-Wno-unused-parameter
	-Wno-missing-braces						# The StarLet's $ASCINI/OPTS_NULL macro style
)

string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type)			# Additional definitons for DEBUG builds
if (build_type STREQUAL debug)
	add_definitions( -D_DEBUG=1 -D__TRACE__=1 )
	add_compile_options( -g3 -O0 -fno-omit-frame-pointer )
endif()

#
#   An additional build type with the run-time checking of memory access and UB:
#	$ cmake ../ -DCMAKE_BUILD_TYPE=Asan
#
if (build_type STREQUAL asan)
	add_definitions( -D_DEBUG=1 -D__TRACE__=1 )
	add_compile_options( -g3 -O1 -fsanitize=address,undefined -fno-omit-frame-pointer )
	add_link_options( -fsanitize=address,undefined )
endif()


check_symbol_exists(TIOCSRS485 sys/ioctl.h HAVE_TIOCRS485)
if(HAVE_TIOCRS485)
    add_definitions(-DHAVE_TIOCRS485)
endif()


include_directories (
	src/
	${LIBCONFIG_INCLUDE_DIRS}
)

set ( T2R_SOURCES                                               # TOOTOO Router
	src/t2r_defs.h
	src/t2r_msgs.h
	src/t2r_net.c
	src/t2r_tty.c
	src/t2r_main.c
	src/t2r_modbus.h
	src/t2r_modbus.c
	src/t2r_ctl.c
)


add_executable ( ${T2R_TARGET} ${T2R_SOURCES})
target_link_libraries( ${T2R_TARGET} StarLet::starlet pthread rt ${T2R_LIBCONFIG_LINK})

#
#   mbus-ctl - the monitoring and control utility: talks to the gateway over its local control socket
#   (see src/t2r_ctl.c); it shares the message catalogue (src/t2r_msgs.h) with the gateway.
#
set ( T2R_CTL_TARGET    "mbus-ctl" )

add_executable ( ${T2R_CTL_TARGET} src/mbus_ctl.c src/t2r_msgs.h src/t2r_defs.h)
target_link_libraries( ${T2R_CTL_TARGET} StarLet::starlet pthread)


#------------------------------------------------------------------------------------------------------
#	Installation:	sbin/mbusgw-t2r, sbin/mbus-ctl
#			etc/mbusgw-t2r/*.conf	- the existing configuration files are never overwritten
#						  (etc/*.conf under /opt/: the tree is self-contained)
#			share/doc/mbusgw-t2r/	- README, BLOCKNOTE, the reference copies of the configs,
#						  the FTDI udev rule
#
if (CMAKE_INSTALL_PREFIX MATCHES "^/opt/")
	set (T2R_CONFDIR "${CMAKE_INSTALL_PREFIX}/etc")
else ()
	set (T2R_CONFDIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/mbusgw-t2r")
endif ()

install (TARGETS ${T2R_TARGET} ${T2R_CTL_TARGET}
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)

install (FILES
	README.md
	README_RU.md
	BLOCKNOTE.md
	modbus-t2r.conf
	modbus-t2r_settings.conf
	docs/UserGuide_EN.md
	docs/UserGuide_RU.md
	docs/UserGuide_EN.pdf
	docs/UserGuide_RU.pdf
	contrib/99-ftdi-latency.rules
	contrib/mbusgw-t2r.service
	DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

#	The configuration files are put to etc/ only when they are absent: an update must not
#	destroy the has been tuned working configuration.
install (CODE "
	file (MAKE_DIRECTORY \"\$ENV{DESTDIR}${T2R_CONFDIR}\")
	foreach (l_conf modbus-t2r.conf modbus-t2r_settings.conf)
		set (l_dst \"\$ENV{DESTDIR}${T2R_CONFDIR}/\${l_conf}\")
		if (NOT EXISTS \"\${l_dst}\")
			file (INSTALL \"${CMAKE_CURRENT_SOURCE_DIR}/\${l_conf}\"
				DESTINATION \"${T2R_CONFDIR}\")	# file (INSTALL) adds DESTDIR itself
		else ()
			message (STATUS \"Keeping the existing \${l_dst}\")
		endif ()
	endforeach ()
")

#	The systemd unit goes to /usr/lib/systemd/system only for the package: 'make install' to /opt/
#	does not touch the system directories.
option(T2R_SYSTEMD_UNIT "Install the systemd unit (for the .deb package)" OFF)

if (T2R_SYSTEMD_UNIT)
	install (FILES contrib/mbusgw-t2r.service DESTINATION /usr/lib/systemd/system)

	#  The package puts mbus-ctl into PATH: /usr/sbin/mbus-ctl -> <prefix>/sbin/mbus-ctl
	#  (the gateway itself is started by the unit and stays under the prefix only)
	execute_process (COMMAND ${CMAKE_COMMAND} -E create_symlink
		"${CMAKE_INSTALL_FULL_SBINDIR}/${T2R_CTL_TARGET}"
		"${CMAKE_CURRENT_BINARY_DIR}/${T2R_CTL_TARGET}.link")	# file (CREATE_LINK) is 3.14+
	install (FILES "${CMAKE_CURRENT_BINARY_DIR}/${T2R_CTL_TARGET}.link"
		DESTINATION /usr/sbin RENAME ${T2R_CTL_TARGET})
endif ()

#	'make uninstall': removes everything from install_manifest.txt; the configuration files
#	which were kept (not overwritten) at the install time are not touched.
add_custom_target (uninstall
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake
	COMMENT "Uninstalling by install_manifest.txt"
)


add_custom_target(files_config SOURCES
	modbus-t2r.conf
	modbus-t2r_settings.conf

	README.md
	BLOCKNOTE.md
)


#------------------------------------------------------------------------------------------------------
#	The .deb package: 'cpack -G DEB' in the build directory. The configuration files are conffiles -
#	dpkg keeps the has been tuned ones on an upgrade; the unit is installed disabled.
#
configure_file (cmake/deb/conffiles.in ${CMAKE_CURRENT_BINARY_DIR}/deb/conffiles @ONLY)

set (CPACK_GENERATOR			"DEB")
set (CPACK_PACKAGE_NAME			"${PROJECT_NAME}")
set (CPACK_PACKAGE_VENDOR		"StarLet Squad")
set (CPACK_PACKAGE_CONTACT		"Ruslan R. Laishev <zator@yandex.ru>")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY	"A yet another gateway for MODBUS (TCP to RTU)")
set (CPACK_PACKAGING_INSTALL_PREFIX	"${CMAKE_INSTALL_PREFIX}")
set (CPACK_DEBIAN_FILE_NAME		DEB-DEFAULT)		# <name>_<version>-<release>_<arch>.deb
set (CPACK_DEBIAN_PACKAGE_RELEASE	1)
set (CPACK_DEBIAN_PACKAGE_SECTION	"net")
set (CPACK_DEBIAN_PACKAGE_HOMEPAGE	"https://github.com/napilab/mbus-gw-t2r")
set (CPACK_DEBIAN_PACKAGE_SHLIBDEPS	ON)			# libc6 (>= ...) by dpkg-shlibdeps
set (CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
	${CMAKE_CURRENT_BINARY_DIR}/deb/conffiles
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/deb/postinst
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/deb/prerm
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/deb/postrm
)
set (CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)

#	A cross build names the architecture of the package: dpkg of the build host would give its own
set (T2R_DEB_ARCH "" CACHE STRING "The Debian architecture of the .deb of a cross build (mipsel, mips64el)")

if (T2R_DEB_ARCH)
	set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${T2R_DEB_ARCH}")
endif ()

include (CPack)
