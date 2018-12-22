include_directories(../include)

if(FIPS)
  add_executable(
    cavp

    cavp_main.cc

    cavp_aes_gcm_test.cc
    cavp_aes_test.cc
    cavp_ctr_drbg_test.cc
    cavp_ecdsa2_keypair_test.cc
    cavp_ecdsa2_pkv_test.cc
    cavp_ecdsa2_siggen_test.cc
    cavp_ecdsa2_sigver_test.cc
    cavp_hmac_test.cc
    cavp_kas_test.cc
    cavp_keywrap_test.cc
    cavp_rsa2_keygen_test.cc
    cavp_rsa2_siggen_test.cc
    cavp_rsa2_sigver_test.cc
    cavp_sha_monte_test.cc
    cavp_sha_test.cc
    cavp_tdes_test.cc
    cavp_tlskdf_test.cc

    cavp_test_util.cc

    $<TARGET_OBJECTS:test_support>
  )

  add_dependencies(cavp global_target)

  add_executable(
    test_fips

    test_fips.c
    $<TARGET_OBJECTS:test_support>
  )

  add_dependencies(test_fips global_target)

  target_link_libraries(cavp crypto)
  target_link_libraries(test_fips crypto)
endif()
