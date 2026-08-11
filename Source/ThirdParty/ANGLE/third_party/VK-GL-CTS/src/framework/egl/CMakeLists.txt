# EGL utilities

add_subdirectory(wrapper)

# GLContextFactory uses glu
include_directories(../opengl)

set(EGLUTIL_SRCS
	egluCallLogWrapper.cpp
	egluCallLogWrapper.hpp
	egluConfigFilter.cpp
	egluConfigFilter.hpp
	egluConfigInfo.cpp
	egluConfigInfo.hpp
	egluDefs.cpp
	egluDefs.hpp
	egluGLContextFactory.cpp
	egluGLContextFactory.hpp
	egluGLFunctionLoader.cpp
	egluGLFunctionLoader.hpp
	egluGLUtil.cpp
	egluGLUtil.hpp
	egluNativeDisplay.cpp
	egluNativeDisplay.hpp
	egluNativePixmap.cpp
	egluNativePixmap.hpp
	egluNativeWindow.cpp
	egluNativeWindow.hpp
	egluPlatform.cpp
	egluPlatform.hpp
	egluStaticESLibrary.cpp
	egluStaticESLibrary.hpp
	egluStrUtil.cpp
	egluStrUtil.hpp
	egluUtil.cpp
	egluUtil.hpp
	egluUnique.hpp
	egluUnique.cpp
	)

add_library(eglutil STATIC ${EGLUTIL_SRCS})
target_link_libraries(eglutil tcutil eglwrapper glutil glwrapper ${DEQP_EGL_LIBRARIES})
