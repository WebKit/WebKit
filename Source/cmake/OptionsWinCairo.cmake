set(WTF_PLATFORM_WIN_CAIRO 1)

include(OptionsWin)

find_package(Cairo 1.14.4 REQUIRED)
find_package(CURL 7.45.0 REQUIRED)
find_package(JPEG REQUIRED)
find_package(LibXml2 2.8.0 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PNG REQUIRED)
find_package(Sqlite REQUIRED)
find_package(ZLIB REQUIRED)

if (ENABLE_XSLT)
    find_package(LibXslt 1.1.7 REQUIRED)
endif ()

SET_AND_EXPOSE_TO_BUILD(USE_CAIRO ON)

set(USE_CF 1)
set(USE_CURL 1)
set(USE_ICU_UNICODE 1)
set(USE_TEXTURE_MAPPER_GL 1)

set(ENABLE_GRAPHICS_CONTEXT_3D 1)

add_definitions(-DWTF_PLATFORM_WIN_CAIRO=1)
