#++
#	FACILITY: A yet another gateway for MODBUS (TCP to RTU)
#
#	DESCRIPTION: The <uninstall> target helper: removes every file which is listed in the
#		install_manifest.txt of the current build directory, except configuration files.
#
#	AUTHOR: StarLet Squad and Ruslan R. Laishev (AKA: BadAss sysman)
#
#	CREATION DATE: 25-AUG-2026
#--

if (NOT EXISTS "${CMAKE_BINARY_DIR}/install_manifest.txt")
	message (FATAL_ERROR "No install_manifest.txt in ${CMAKE_BINARY_DIR}: nothing has been installed from here")
endif ()

file (STRINGS "${CMAKE_BINARY_DIR}/install_manifest.txt" l_files)

foreach (l_file ${l_files})
	if (l_file STREQUAL "${T2R_CONFDIR}/modbus-t2r.conf" OR
	    l_file STREQUAL "${T2R_CONFDIR}/modbus-t2r_settings.conf")
		message (STATUS "Keeping configuration: ${l_file}")
		continue ()
	endif ()
	if (EXISTS "$ENV{DESTDIR}${l_file}" OR IS_SYMLINK "$ENV{DESTDIR}${l_file}")
		message (STATUS "Removing: $ENV{DESTDIR}${l_file}")
		file (REMOVE "$ENV{DESTDIR}${l_file}")
	else ()
		message (STATUS "Missing (skipped): $ENV{DESTDIR}${l_file}")
	endif ()
endforeach ()
