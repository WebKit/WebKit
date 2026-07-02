# Platform port library

# Target file may define TCUTIL_PLATFORM_SRCS
if (NOT DEFINED TCUTIL_PLATFORM_SRCS)
	if (DE_OS_IS_WIN32)
		set(TCUTIL_PLATFORM_SRCS
			win32/tcuWin32Platform.hpp
			win32/tcuWin32Platform.cpp
			win32/tcuWGLContextFactory.hpp
			win32/tcuWGLContextFactory.cpp
			win32/tcuWGL.hpp
			win32/tcuWGL.cpp
			win32/tcuWin32API.h
			win32/tcuWin32Window.cpp
			win32/tcuWin32Window.hpp
			win32/tcuWin32EGLNativeDisplayFactory.hpp
			win32/tcuWin32EGLNativeDisplayFactory.cpp
			win32/tcuWin32VulkanPlatform.hpp
			win32/tcuWin32VulkanPlatform.cpp
			)

	elseif ((DE_OS_IS_UNIX OR DE_OS_IS_OSX) AND (DEQP_USE_X11 OR DEQP_USE_WAYLAND OR DEQP_USE_HEADLESS))

		set(TCUTIL_PLATFORM_SRCS
			lnx/tcuLnx.cpp
			lnx/tcuLnx.hpp
			lnx/tcuLnxPlatform.cpp
			lnx/tcuLnxPlatform.hpp
			lnx/tcuLnxEglPlatform.hpp
			lnx/tcuLnxEglPlatform.cpp
			lnx/tcuLnxVulkanPlatform.hpp
			lnx/tcuLnxVulkanPlatform.cpp
		)

		include_directories(lnx)

		find_package(PkgConfig REQUIRED)

		if (DEQP_USE_X11)
			add_definitions(-DDEQP_SUPPORT_X11=1)
			include_directories(lnx/X11)

			message(STATUS "DEQP_SUPPORT_GLX        = ${DEQP_SUPPORT_GLX}")

			if (DEQP_SUPPORT_GLX)
				add_definitions(-DDEQP_SUPPORT_GLX=1)
			endif ()

			set(TCUTIL_PLATFORM_SRCS
				${TCUTIL_PLATFORM_SRCS}
				lnx/X11/tcuLnxX11.cpp
				lnx/X11/tcuLnxX11.hpp
				lnx/X11/tcuLnxX11EglDisplayFactory.hpp
				lnx/X11/tcuLnxX11EglDisplayFactory.cpp
				)

			if (DEQP_USE_XCB)
				set(TCUTIL_PLATFORM_SRCS
					${TCUTIL_PLATFORM_SRCS}
					lnx/X11/tcuLnxX11Xcb.hpp
					lnx/X11/tcuLnxX11Xcb.cpp
					)
			endif()

			if (DEQP_SUPPORT_GLX)
				pkg_check_modules(GLX glx REQUIRED)
				set(TCUTIL_PLATFORM_SRCS
					${TCUTIL_PLATFORM_SRCS}
					lnx/X11/tcuLnxX11GlxPlatform.hpp
					lnx/X11/tcuLnxX11GlxPlatform.cpp
				)
			endif()
		endif()

		if (DEQP_USE_WAYLAND)
			add_definitions(-DDEQP_SUPPORT_WAYLAND=1)
			include_directories(lnx/wayland)

			set(DEQP_XDG_SHELL_GEN_OUTPUTS_DIR ${PROJECT_BINARY_DIR}/framework/platform)
			set(DEQP_XDG_SHELL_OUTPUTS
				${DEQP_XDG_SHELL_GEN_OUTPUTS_DIR}/xdg-shell.c
				${DEQP_XDG_SHELL_GEN_OUTPUTS_DIR}/xdg-shell.h)

			if (WAYLAND_XDG_SHELL_SRC_DIR)
				# Allow xdg-shell.c and xdg-shell.h to be source files specified in an external directory
				configure_file(${WAYLAND_XDG_SHELL_SRC_DIR}/xdg-shell.c ${DEQP_XDG_SHELL_GEN_OUTPUTS_DIR}/xdg-shell.c COPYONLY)
				configure_file(${WAYLAND_XDG_SHELL_SRC_DIR}/xdg-shell.h ${DEQP_XDG_SHELL_GEN_OUTPUTS_DIR}/xdg-shell.h COPYONLY)
			else ()
				if (NOT WAYLAND_PROTOCOLS_DIR)
					pkg_check_modules(WAYLAND_PROTOCOLS_DIR wayland-protocols REQUIRED)
					pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
				endif()

				if (NOT WAYLAND_SCANNER)
					pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)
				endif()

				set(DEQP_XDG_SHELL_PROTOCOL ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml)
				add_custom_command(
					COMMAND ${WAYLAND_SCANNER} ARGS client-header ${DEQP_XDG_SHELL_PROTOCOL} xdg-shell.h
					COMMAND ${WAYLAND_SCANNER} ARGS public-code ${DEQP_XDG_SHELL_PROTOCOL} xdg-shell.c
					DEPENDS ${DEQP_XDG_SHELL_PROTOCOL}
					OUTPUT ${DEQP_XDG_SHELL_OUTPUTS})
			endif()

			add_custom_target(
				deqp-xdg-shell ALL
				DEPENDS ${DEQP_XDG_SHELL_OUTPUTS})

			set(TCUTIL_PLATFORM_SRCS
				${TCUTIL_PLATFORM_SRCS}
				lnx/wayland/tcuLnxWayland.hpp
				lnx/wayland/tcuLnxWayland.cpp
				lnx/wayland/tcuLnxWaylandEglDisplayFactory.cpp
				lnx/wayland/tcuLnxWaylandEglDisplayFactory.hpp
				${DEQP_XDG_SHELL_GEN_OUTPUTS_DIR}/xdg-shell.c
				${DEQP_XDG_SHELL_GEN_OUTPUTS_DIR}/xdg-shell.h
				)
			include_directories(${DEQP_XDG_SHELL_GEN_OUTPUTS_DIR})
			include_directories(wayland)
		endif()

		if (DEQP_USE_HEADLESS)
			add_definitions(-DDEQP_SUPPORT_HEADLESS=1)
		endif()

	elseif (DE_OS_IS_UNIX AND DEQP_USE_SURFACELESS)
		add_definitions(-DDEQP_SURFACELESS=1)
		set(TCUTIL_PLATFORM_SRCS
			surfaceless/tcuSurfacelessPlatform.hpp
			surfaceless/tcuSurfacelessPlatform.cpp
			)

	elseif (DE_OS_IS_ANDROID)
		set(TCUTIL_PLATFORM_SRCS
			android/tcuAndroidExecService.cpp
			android/tcuAndroidExecService.hpp
			)

		if (DE_ANDROID_API GREATER 8)
			# Add NativeActivity code
			set(TCUTIL_PLATFORM_SRCS
				${TCUTIL_PLATFORM_SRCS}
				android/tcuAndroidAssets.cpp
				android/tcuAndroidAssets.hpp
				android/tcuAndroidNativeActivity.cpp
				android/tcuAndroidNativeActivity.hpp
				android/tcuAndroidPlatform.cpp
				android/tcuAndroidPlatform.hpp
				android/tcuAndroidRenderActivity.cpp
				android/tcuAndroidRenderActivity.hpp
				android/tcuAndroidTestActivity.cpp
				android/tcuAndroidTestActivity.hpp
				android/tcuAndroidUtil.cpp
				android/tcuAndroidUtil.hpp
				android/tcuAndroidWindow.cpp
				android/tcuAndroidWindow.hpp
				)
		endif ()

	elseif (DE_OS_IS_IOS)
		set(TCUTIL_PLATFORM_SRCS
			ios/tcuIOSApp.mm
			ios/tcuIOSApp.h
			ios/tcuIOSPlatform.mm
			ios/tcuIOSPlatform.hh
			)

	elseif (DE_OS_IS_OSX)
		set(TCUTIL_PLATFORM_SRCS
			osx/tcuOSXPlatform.cpp
			osx/tcuOSXPlatform.hpp
			osx/tcuOSXVulkanPlatform.cpp
			osx/tcuOSXVulkanPlatform.hpp
			osx/tcuOSXMetalView.mm
			osx/tcuOSXMetalView.hpp
			)
		list(APPEND TCUTIL_PLATFORM_LIBS ${QUARTZCORE_LIBRARY} ${COCOA_LIBRARY})

	elseif (DE_OS_IS_FUCHSIA)
		set(TCUTIL_PLATFORM_SRCS
			fuchsia/tcuFuchsiaPlatform.cpp
			)

	else ()
		set(TCUTIL_PLATFORM_SRCS
			vanilla/tcuVanillaPlatform.cpp
			)

	endif ()
endif ()

include_directories(${PROJECT_BINARY_DIR}/external/vulkancts/framework/vulkan)

add_library(tcutil-platform STATIC ${TCUTIL_PLATFORM_SRCS})
if (DEQP_USE_WAYLAND)
	add_dependencies(tcutil-platform deqp-xdg-shell)
endif()

# Add vkutil to the deps before tcutil so that it picks up the c++11 dependencies
target_link_libraries(tcutil-platform vkutil)

target_link_libraries(tcutil-platform tcutil ${TCUTIL_PLATFORM_LIBS})

# Always link to glutil as some platforms such as Win32 always support GL
target_link_libraries(tcutil-platform glutil)

# Always link to eglutil
target_link_libraries(tcutil-platform eglutil)

# X11 libraries
if (DEQP_USE_X11)
	find_package(X11 REQUIRED)
	target_link_libraries(tcutil-platform ${X11_LIBRARIES})
	if (DEQP_USE_XCB)
		find_package(XCB REQUIRED)
		target_link_libraries(tcutil-platform ${XCB_LIBRARIES})
		target_compile_definitions(tcutil-platform PUBLIC DEQP_SUPPORT_XCB=1)
	endif ()
	if (DEQP_SUPPORT_GLX)
	  # GLX functions don't currently have wrappers, so link directly to libGL.
	  target_link_libraries(tcutil-platform GL)
	endif ()
	if (DEQP_USE_WAYLAND)
		find_package(Wayland REQUIRED)
		target_link_libraries(tcutil-platform ${WAYLAND_LIBRARIES})
		target_compile_definitions(tcutil-platform PUBLIC DEQP_SUPPORT_WAYLAND=1)
	endif ()
endif ()

if (DE_OS_IS_WIN32)
	target_link_libraries(tcutil-platform "version")
endif()

# Add similar library for Vulkan SC ( uses same code as vkutil, but with different INL files. These
# are picked up via the version of vkutil that is linked against )
add_library(tcutil-sc-platform STATIC ${TCUTIL_PLATFORM_SRCS})

# Add vkscutil to the deps before tcutil so that it picks up the c++11 dependencies
target_link_libraries(tcutil-sc-platform vkscutil)

target_link_libraries(tcutil-sc-platform tcutil ${TCUTIL_PLATFORM_LIBS})

# Always link to glutil as some platforms such as Win32 always support GL
target_link_libraries(tcutil-sc-platform glutil)

# Always link to eglutil
target_link_libraries(tcutil-sc-platform eglutil)

# X11 libraries
if (DEQP_USE_X11)
	find_package(X11 REQUIRED)
	target_link_libraries(tcutil-sc-platform ${X11_LIBRARIES})
	if (DEQP_USE_XCB)
		find_package(XCB REQUIRED)
		target_link_libraries(tcutil-sc-platform ${XCB_LIBRARIES})
		target_compile_definitions(tcutil-sc-platform PUBLIC DEQP_SUPPORT_XCB=1)
	endif ()
	if (DEQP_SUPPORT_GLX)
	  # GLX functions don't currently have wrappers, so link directly to libGL.
	  target_link_libraries(tcutil-sc-platform GL)
	endif ()
	if (DEQP_USE_WAYLAND)
		find_package(Wayland REQUIRED)
		target_link_libraries(tcutil-sc-platform ${WAYLAND_LIBRARIES})
		target_compile_definitions(tcutil-sc-platform PUBLIC DEQP_SUPPORT_WAYLAND=1)
	endif ()
endif ()

if (DE_OS_IS_WIN32)
	target_link_libraries(tcutil-sc-platform "version")
endif()
