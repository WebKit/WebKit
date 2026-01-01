list(APPEND CMAKE_MODULE_PATH "${THIRDPARTY_DIR}/corrosion/cmake")
include(Corrosion)

corrosion_import_crate(MANIFEST_PATH ${THIRDPARTY_DIR}/mdns-service-rs/Cargo.toml
  OVERRIDE_CRATE_TYPE mdns_service=staticlib
  FLAGS --quiet
)
list(APPEND WebKit_LIBRARIES mdns_service)
list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES "${THIRDPARTY_DIR}/mdns-service-rs/")

list(APPEND WebKit_SOURCES
  NetworkProcess/webrtc/NetworkMDNSRegisterLinux.cpp
)
