set(TESTAPI_OBJC_SOURCES
    ../API/tests/CurrentThisInsideBlockGetterTest.mm
    ../API/tests/DateTests.mm
    ../API/tests/JSExportTests.mm
    ../API/tests/Regress141275.mm
    ../API/tests/Regress141809.mm
    ../API/tests/testapi.mm
)
list(APPEND TESTAPI_SOURCES ${TESTAPI_OBJC_SOURCES})

set_source_files_properties(${TESTAPI_OBJC_SOURCES} PROPERTIES COMPILE_FLAGS -fobjc-arc)
