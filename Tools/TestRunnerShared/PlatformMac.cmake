add_definitions(-DJSC_API_AVAILABLE\\\(...\\\)=)

list(APPEND TestRunnerShared_INTERFACE_INCLUDE_DIRECTORIES
    ${TestRunnerShared_DIR}/EventSerialization/mac
    ${TestRunnerShared_DIR}/cocoa
    ${TestRunnerShared_DIR}/mac
    ${TestRunnerShared_DIR}/spi
)
