# EGL Wrapper

set(EGLWRAPPER_SRCS
	eglwDefs.cpp
	eglwDefs.hpp
	eglwEnums.hpp
	eglwFunctions.hpp
	eglwFunctions.cpp
	eglwLibrary.hpp
	eglwLibrary.cpp
	eglwWrapper.hpp
	eglwWrapper.cpp
	eglw.h
	)

set(EGLWRAPPER_LIBS
	debase
	deutil
	decpp
	${DEQP_EGL_LIBRARIES}
	)

add_library(eglwrapper STATIC ${EGLWRAPPER_SRCS})
target_link_libraries(eglwrapper ${EGLWRAPPER_LIBS})
