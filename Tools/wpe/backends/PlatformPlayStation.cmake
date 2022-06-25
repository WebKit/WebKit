list(APPEND WPEToolingBackends_SOURCES
    playstation/HeadlessViewBackendPlayStation.cpp
)

list(APPEND WPEToolingBackends_LIBRARIES WPE::PlayStation)
list(APPEND WPEToolingBackends_DEFINITIONS WPE_BACKEND_PLAYSTATION)
list(APPEND WPEToolingBackends_PRIVATE_DEFINITIONS WPE_BACKEND="")
