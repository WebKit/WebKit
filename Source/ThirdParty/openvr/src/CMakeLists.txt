set(OPENVR_SOURCE_DIR ${CMAKE_SOURCE_DIR}/Source/ThirdParty/openvr)

set( LIBNAME "openvr_api" )

set(EXCEPTION_FLAGS "-fexceptions")
add_definitions(${EXCEPTION_FLAGS})

# Set some properies for specific files.
if(APPLE)
  set(CMAKE_MACOSX_RPATH 1)
  if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set_source_files_properties(vrcommon/pathtools_public.cpp vrcommon/vrpathregistry_public.cpp PROPERTIES COMPILE_FLAGS "-x objective-c++")
  endif()
  if(BUILD_SHARED OR BUILD_FRAMEWORK)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    mark_as_advanced(FOUNDATION_FRAMEWORK)
    set(EXTRA_LIBS ${EXTRA_LIBS} ${FOUNDATION_FRAMEWORK})
  endif(BUILD_SHARED OR BUILD_FRAMEWORK)
elseif(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions( -DWIN64 )
    set( LIBNAME "openvr_api64" )
  endif()
endif()

# Add include folders.
include_directories(${OPENVR_SOURCE_DIR}/src ${OPENVR_SOURCE_DIR}/headers ${OPENVR_SOURCE_DIR}/src/vrcommon)

if(USE_CUSTOM_LIBCXX)
       link_directories(
              ${LIBCXX_LIB_DIR}
       )
endif()

# Set the source group and files.
set(CORE_FILES
       openvr_api_public.cpp
       jsoncpp.cpp
)
set(VRCOMMON_FILES
       vrcommon/dirtools_public.cpp
       vrcommon/envvartools_public.cpp
       vrcommon/pathtools_public.cpp
       vrcommon/sharedlibtools_public.cpp
       vrcommon/hmderrors_public.cpp
       vrcommon/vrpathregistry_public.cpp
       vrcommon/strtools_public.cpp
)

set(SOURCE_FILES
       ${CORE_FILES}
       ${VRCOMMON_FILES}
)

source_group("Src" FILES
       ${CORE_FILES}
)

source_group("VRCommon" FILES
       ${VRCOMMON_FILES}
)

# Build the library.
if(BUILD_SHARED)
       add_library(${LIBNAME} SHARED ${SOURCE_FILES})
elseif(BUILD_FRAMEWORK)
       set( LIBNAME "OpenVR" )
       add_library( ${LIBNAME} 
            SHARED ${SOURCE_FILES}
            ${OPENVR_SOURCE_DIR}/headers/openvr.h
            ${OPENVR_SOURCE_DIR}/headers/openvr_api.cs
            ${OPENVR_SOURCE_DIR}/headers/openvr_api.json
            ${OPENVR_SOURCE_DIR}/headers/openvr_capi.h
            ${OPENVR_SOURCE_DIR}/headers/openvr_driver.h
       )
       set_target_properties(OpenVR PROPERTIES
              FRAMEWORK TRUE
              FRAMEWORK_VERSION A
              MACOSX_FRAMEWORK_IDENTIFIER com.valvesoftware.OpenVR.framework
              MACOSX_FRAMEWORK_INFO_PLIST ${OPENVR_SOURCE_DIR}/src/Info.plist
              # "current version" in semantic format in Mach-O binary file
              VERSION 1.0.6
              # "compatibility version" in semantic format in Mach-O binary file
              SOVERSION 1.0.0
              PUBLIC_HEADER "${OPENVR_SOURCE_DIR}/headers/openvr.h;${OPENVR_SOURCE_DIR}/headers/openvr_api.cs;${OPENVR_SOURCE_DIR}/headers/openvr_api.json;${OPENVR_SOURCE_DIR}/headers/openvr_capi.h;${OPENVR_SOURCE_DIR}/headers/openvr_driver.h"
              LINKER_LANGUAGE CXX
       )
else()
       add_library(${LIBNAME} STATIC ${SOURCE_FILES})
endif()

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(${LIBNAME} -Wno-implicit-fallthrough
                                           -Wno-unknown-pragmas
                                           -Wno-unused-parameter
                                           -Wno-unused-variable)
endif()

if(USE_CUSTOM_LIBCXX)
       set(EXTRA_LIBS ${EXTRA_LIBS} c++ c++abi)
endif()

target_link_libraries(${LIBNAME} ${EXTRA_LIBS})

install(TARGETS ${LIBNAME} DESTINATION lib)
