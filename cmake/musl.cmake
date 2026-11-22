#++
#	FACILITY: A yet another gateway
#
#	DESCRIPTION: A CMake toolchain file to build against the musl C library instead of the
#		glibc one - the case of OpenWrt, Alpine and of the most embedded rootfs images.
#
#		musl ships its own headers but NOT the kernel ones (linux/serial.h, asm/ioctls.h
#		and friends), so the kernel header directories are appended by -idirafter: this
#		way the musl headers always win and /usr/include is consulted only for what musl
#		does not provide. A plain -I would poison the build with the glibc headers.
#
#	USAGE:
#		$ cmake ../ -DCMAKE_TOOLCHAIN_FILE=../cmake/musl.cmake -DCMAKE_BUILD_TYPE=Release
#
#		The dependencies (StarLet, yaLIBCFG) must be built for musl too and pointed at
#		by -DCMAKE_PREFIX_PATH=<musl prefix>.
#
#	AUTHOR: StarLet Squad and Ruslan R. Laishev (AKA: BadAss sysman)
#
#	CREATION DATE: 25-AUG-2026
#--

set (CMAKE_SYSTEM_NAME Linux)

#
#   Setting CMAKE_SYSTEM_NAME turns the build into a "cross" one, so CMAKE_SYSTEM_PROCESSOR is
#   no longer filled in automatically - and it goes into the revision banner of the program.
#   For a native musl build it is the processor of the host; for a real cross build the caller
#   passes -DCMAKE_SYSTEM_PROCESSOR=<arch>.
#
if (NOT CMAKE_SYSTEM_PROCESSOR)
	execute_process (COMMAND uname -m OUTPUT_VARIABLE l_uname_m OUTPUT_STRIP_TRAILING_WHITESPACE)
	set (CMAKE_SYSTEM_PROCESSOR "${l_uname_m}")
endif ()

if (NOT CMAKE_C_COMPILER)
	find_program (MUSL_CC NAMES musl-gcc ${CMAKE_SYSTEM_PROCESSOR}-linux-musl-gcc musl-clang)

	if (NOT MUSL_CC)
		message (FATAL_ERROR "No musl compiler has been found: install musl-tools or a musl cross toolchain")
	endif ()

	set (CMAKE_C_COMPILER ${MUSL_CC})
endif ()

#
#   The kernel headers: -idirafter keeps them BELOW the musl ones in the search order
#
#
#   On a Debian/Ubuntu host the <asm/*.h> headers live in the multiarch directory
#   (/usr/include/x86_64-linux-gnu/asm), so it is looked up and appended as well.
#
if (NOT KERNEL_HEADER_DIRS)
	set (KERNEL_HEADER_DIRS "/usr/include")

	file (GLOB l_asm_dirs "/usr/include/*/asm/types.h")

	foreach (l_hit ${l_asm_dirs})
		get_filename_component (l_dir "${l_hit}" DIRECTORY)		# .../asm
		get_filename_component (l_dir "${l_dir}" DIRECTORY)		# .../
		list (APPEND KERNEL_HEADER_DIRS "${l_dir}")
	endforeach ()
endif ()

set (KERNEL_HEADER_DIRS "${KERNEL_HEADER_DIRS}" CACHE STRING "Where the kernel headers (linux/*.h, asm/*.h) live")

foreach (l_dir ${KERNEL_HEADER_DIRS})
	set (CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -idirafter ${l_dir}")
endforeach ()

set (CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -D_GNU_SOURCE")
