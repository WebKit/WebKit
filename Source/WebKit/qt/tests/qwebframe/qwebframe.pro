isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../../..
include(../tests.pri)
exists($${TARGET}.qrc):RESOURCES += $${TARGET}.qrc

symbian {
    TARGET.UID3 = 0xE5CDF386
    qwebkitApiTestResources.path = private/$$replace(TARGET.UID3, 0x,)/resources
    qwebkitApiTestResources.sources += ../resources/image2.png
    DEPLOYMENT += qwebkitApiTestResources
    DEFINES += TESTS_SOURCE_DIR=\"/private/$$replace(TARGET.UID3, 0x,)/\"
}
