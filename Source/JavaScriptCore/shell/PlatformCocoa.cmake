set(testapi_OBJC_SOURCES
    ../API/tests/CurrentThisInsideBlockGetterTest.mm
    ../API/tests/DateTests.mm
    ../API/tests/JSExportTests.mm
    ../API/tests/JSWrapperMapTests.mm
    ../API/tests/Regress141275.mm
    ../API/tests/Regress141809.mm
    ../API/tests/testapi.mm
)
list(APPEND testapi_SOURCES ${testapi_OBJC_SOURCES})
set_source_files_properties(${testapi_OBJC_SOURCES} PROPERTIES
    COMPILE_FLAGS -fobjc-arc
    SKIP_PRECOMPILE_HEADERS ON
)

WEBKIT_GENERATE_ENTITLEMENTS(jsc USING ../Scripts/process-entitlements.sh)
if (DEVELOPER_MODE)
    WEBKIT_GENERATE_ENTITLEMENTS(testapi USING ../Scripts/process-entitlements.sh)
    WEBKIT_GENERATE_ENTITLEMENTS(testRegExp USING ../Scripts/process-entitlements.sh)
    WEBKIT_GENERATE_ENTITLEMENTS(testmasm USING ../Scripts/process-entitlements.sh)
    WEBKIT_GENERATE_ENTITLEMENTS(testb3 USING ../Scripts/process-entitlements.sh)
    WEBKIT_GENERATE_ENTITLEMENTS(testair USING ../Scripts/process-entitlements.sh)
    WEBKIT_GENERATE_ENTITLEMENTS(testdfg USING ../Scripts/process-entitlements.sh)
endif ()

