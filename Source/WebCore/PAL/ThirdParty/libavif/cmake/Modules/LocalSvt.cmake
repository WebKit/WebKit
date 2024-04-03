set(LIB_FILENAME "${AVIF_SOURCE_DIR}/ext/SVT-AV1/Bin/Release/${AVIF_LIBRARY_PREFIX}SvtAv1Enc${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(NOT EXISTS "${LIB_FILENAME}")
    message(FATAL_ERROR "libavif: compiled svt library is missing (in ext/SVT-AV1/Bin/Release), bailing out")
endif()

add_library(SvtAv1Enc STATIC IMPORTED GLOBAL)
set_target_properties(SvtAv1Enc PROPERTIES IMPORTED_LOCATION "${LIB_FILENAME}" AVIF_LOCAL ON)
target_include_directories(SvtAv1Enc INTERFACE "${AVIF_SOURCE_DIR}/ext/SVT-AV1/include")
