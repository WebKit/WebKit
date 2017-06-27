include_directories(../include)

add_library(
  ssl

  bio_ssl.c
  custom_extensions.c
  d1_both.c
  d1_lib.c
  d1_pkt.c
  d1_srtp.c
  dtls_method.c
  dtls_record.c
  handshake_client.c
  handshake_server.c
  s3_both.c
  s3_lib.c
  s3_pkt.c
  ssl_aead_ctx.c
  ssl_asn1.c
  ssl_buffer.c
  ssl_cert.c
  ssl_cipher.c
  ssl_ecdh.c
  ssl_file.c
  ssl_lib.c
  ssl_privkey.c
  ssl_privkey_cc.cc
  ssl_session.c
  ssl_stat.c
  ssl_transcript.c
  ssl_x509.c
  t1_enc.c
  t1_lib.c
  tls_method.c
  tls_record.c
  tls13_both.c
  tls13_client.c
  tls13_enc.c
  tls13_server.c
)

target_link_libraries(ssl crypto)

add_executable(
  ssl_test

  ssl_test.cc

  $<TARGET_OBJECTS:gtest_main>
  $<TARGET_OBJECTS:test_support>
)

target_link_libraries(ssl_test ssl crypto gtest)
if (WIN32)
  target_link_libraries(ssl_test ws2_32)
endif()
add_dependencies(all_tests ssl_test)
