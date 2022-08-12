# WebKit build rules for unifdef

set(UNIFDEF_DIR "${THIRDPARTY_DIR}/unifdef")

set(UNIFDEF_SOURCES
    ${UNIFDEF_DIR}/unifdef.c
)

add_executable(unifdef ${UNIFDEF_SOURCES})
target_include_directories(unifdef PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(unifdef -Wno-suggest-attribute=format)
endif ()
