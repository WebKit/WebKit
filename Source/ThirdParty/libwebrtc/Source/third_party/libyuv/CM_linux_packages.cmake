# determine the version number from the #define in libyuv/version.h
EXECUTE_PROCESS (
	COMMAND grep --perl-regex --only-matching "(?<=LIBYUV_VERSION )[0-9]+" include/libyuv/version.h
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
	OUTPUT_VARIABLE YUV_VERSION_NUMBER
	OUTPUT_STRIP_TRAILING_WHITESPACE )
SET ( YUV_VER_MAJOR 0 )
SET ( YUV_VER_MINOR 0 )
SET ( YUV_VER_PATCH ${YUV_VERSION_NUMBER} )
SET ( YUV_VERSION ${YUV_VER_MAJOR}.${YUV_VER_MINOR}.${YUV_VER_PATCH} )
MESSAGE ( VERBOSE "Building ver.: ${YUV_VERSION}" )

# is this a 32-bit or 64-bit build?
IF ( CMAKE_SIZEOF_VOID_P EQUAL 8 )
	SET ( YUV_BIT_SIZE 64 )
ELSEIF ( CMAKE_SIZEOF_VOID_P EQUAL 4 )
	SET ( YUV_BIT_SIZE 32 )
ELSE ()
	MESSAGE ( FATAL_ERROR "CMAKE_SIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P}" )
ENDIF ()

# detect if this is a ARM build
STRING (FIND "${CMAKE_CXX_COMPILER}" "arm-linux-gnueabihf-g++" pos)
IF ( ${pos} EQUAL -1 )
	SET ( YUV_CROSS_COMPILE_FOR_ARM7 FALSE )
ELSE ()
	MESSAGE ( "Cross compiling for ARM7" )
	SET ( YUV_CROSS_COMPILE_FOR_ARM7 TRUE )
ENDIF ()
STRING (FIND "${CMAKE_SYSTEM_PROCESSOR}" "arm" pos)
IF ( ${pos} EQUAL -1 )
	SET ( YUV_COMPILE_FOR_ARM7 FALSE )
ELSE ()
	MESSAGE ( "Compiling for ARM" )
	SET ( YUV_COMPILE_FOR_ARM7 TRUE )
ENDIF ()

# setup the sytem name, such as "x86-32", "amd-64", and "arm-32
IF ( ${YUV_CROSS_COMPILE_FOR_ARM7} OR ${YUV_COMPILE_FOR_ARM7} )
	SET ( YUV_SYSTEM_NAME "armhf-${YUV_BIT_SIZE}" )
ELSE ()
	IF ( YUV_BIT_SIZE EQUAL 32 )
		SET ( YUV_SYSTEM_NAME "x86-${YUV_BIT_SIZE}" )
	ELSE ()
		SET ( YUV_SYSTEM_NAME "amd-${YUV_BIT_SIZE}" )
	ENDIF ()
ENDIF ()
MESSAGE ( VERBOSE "Packaging for: ${YUV_SYSTEM_NAME}" )

# define all the variables needed by CPack to create .deb and .rpm packages
SET ( CPACK_PACKAGE_VENDOR					"Frank Barchard" )
SET ( CPACK_PACKAGE_CONTACT					"fbarchard@chromium.org" )
SET ( CPACK_PACKAGE_VERSION					${YUV_VERSION} )
SET ( CPACK_PACKAGE_VERSION_MAJOR			${YUV_VER_MAJOR} )
SET ( CPACK_PACKAGE_VERSION_MINOR			${YUV_VER_MINOR} )
SET ( CPACK_PACKAGE_VERSION_PATCH			${YUV_VER_PATCH} )
SET ( CPACK_RESOURCE_FILE_LICENSE			${PROJECT_SOURCE_DIR}/LICENSE )
SET ( CPACK_SYSTEM_NAME						"linux-${YUV_SYSTEM_NAME}" )
SET ( CPACK_PACKAGE_NAME					"libyuv" )
SET ( CPACK_PACKAGE_DESCRIPTION_SUMMARY		"YUV library" )
SET ( CPACK_PACKAGE_DESCRIPTION				"YUV library and YUV conversion tool" )
SET ( CPACK_DEBIAN_PACKAGE_SECTION			"other" )
SET ( CPACK_DEBIAN_PACKAGE_PRIORITY			"optional" )
SET ( CPACK_DEBIAN_PACKAGE_MAINTAINER		"Frank Barchard <fbarchard@chromium.org>" )
SET ( CPACK_GENERATOR						"DEB;RPM" )

# create the .deb and .rpm files (you'll need build-essential and rpm tools)
INCLUDE( CPack )

