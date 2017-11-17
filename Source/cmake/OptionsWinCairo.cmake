set(WTF_PLATFORM_WIN_CAIRO 1)

include(OptionsWin)

find_package(Cairo 1.14.10 REQUIRED)
find_package(CURL 7.56.1 REQUIRED)
find_package(JPEG 1.5.2 REQUIRED)
find_package(LibXml2 2.9.7 REQUIRED)
find_package(OpenSSL 2.0.0 REQUIRED)
find_package(PNG 1.6.34 REQUIRED)
find_package(Sqlite 3.21.0 REQUIRED)
find_package(ZLIB 1.2.11 REQUIRED)

if (ENABLE_XSLT)
    find_package(LibXslt 1.1.32 REQUIRED)
endif ()

SET_AND_EXPOSE_TO_BUILD(USE_CAIRO ON)
SET_AND_EXPOSE_TO_BUILD(USE_CF ON)

set(USE_CURL 1)
set(USE_ICU_UNICODE 1)
set(USE_TEXTURE_MAPPER_GL 1)

set(ENABLE_GRAPHICS_CONTEXT_3D 1)

set(COREFOUNDATION_LIBRARY CFlite)

add_definitions(-DWTF_PLATFORM_WIN_CAIRO=1)
