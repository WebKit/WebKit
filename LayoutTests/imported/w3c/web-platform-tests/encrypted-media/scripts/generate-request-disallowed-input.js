function runTest(config,qualifier) {
    var tests = [ ], initData, keyId;
    function push_test(keysystem, initDataType, initData, testname) {
        tests.push({ keysystem: keysystem, initDataType: initDataType, initData: initData, testname: testname });
    }

    initData = new Uint8Array(70000);
    push_test(config.keysystem, 'webm', initData, testnamePrefix( qualifier, config.keysystem ) + ', temporary, webm, initData longer than 64Kb characters');

    initData = new Uint8Array(70000);
    push_test(config.keysystem, 'cenc', initData, testnamePrefix( qualifier, config.keysystem ) + ', temporary, cenc, initData longer than 64Kb characters');

    initData = new Uint8Array(70000);
    push_test(config.keysystem, 'keyids', initData, testnamePrefix( qualifier, config.keysystem ) + ', temporary, keyids, initData longer than 64Kb characters');

    // Invalid 'pssh' box as the size specified is larger than what
    // is provided.
    initData = new Uint8Array([
        0x00, 0x00, 0xff, 0xff,                          // size = huge
        0x70, 0x73, 0x73, 0x68,                          // 'pssh'
        0x00,                                            // version = 0
        0x00, 0x00, 0x00,                                // flags
        0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // Common SystemID
        0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
        0x00, 0x00, 0x00, 0x00                           // datasize
    ]);
    push_test(config.keysystem, 'cenc', initData, testnamePrefix( qualifier, config.keysystem ) + ', temporary, cenc, invalid initdata (size too large)');

    // Invalid data as type = 'psss'.
    initData = new Uint8Array([
        0x00, 0x00, 0x00, 0x00,                          // size = 0
        0x70, 0x73, 0x73, 0x73,                          // 'psss'
        0x00,                                            // version = 0
        0x00, 0x00, 0x00,                                // flags
        0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // Common SystemID
        0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
        0x00, 0x00, 0x00, 0x00                           // datasize
    ]);
    push_test(config.keysystem, 'cenc', initData, testnamePrefix( qualifier, config.keysystem ) + ', temporary, cenc, invalid initdata (not pssh)');

    initData = new Uint8Array([
        0x00, 0x00, 0x00, 0x44, 0x70, 0x73, 0x73, 0x68, // BMFF box header (68 bytes, 'pssh')
        0x01, 0x00, 0x00, 0x00,                         // Full box header (version = 1, flags = 0)
        0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, // SystemID
        0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b,
        0x00, 0x00, 0x00, 0x04,                         // KID_count (4) (incorrect)
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, // First KID ("0123456789012345")
        0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, // Second KID ("ABCDEFGHIJKLMNOP")
        0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
        0x00, 0x00, 0x00, 0x00                          // Size of Data (0)
    ]);
    push_test(config.keysystem, 'cenc', initData, testnamePrefix( qualifier, config.keysystem ) + ', temporary, cenc, invalid key id length (4 instead of 2)');

    initData = new Uint8Array([
        0x00, 0x00, 0x00, 0x54, 0x70, 0x73, 0x73, 0x68, // BMFF box header (84 bytes, 'pssh')
        0x01, 0x00, 0x00, 0x00,                         // Full box header (version = 1, flags = 0)
        0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, // SystemID
        0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b,
        0x00, 0x00, 0x00, 0x02,                         // KID_count (2)
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, // First KID ("0123456789012345")
        0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, // Second KID ("ABCDEFGHIJKLMNOP")
        0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
        0x00, 0x00, 0x00, 0x20,                         // Size of Data (32, incorrect)
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50
    ]);
    push_test(config.keysystem, 'cenc', initData, testnamePrefix( qualifier, config.keysystem ) + ', temporary, cenc, invalid data size (32 instead of 16)');

    // Invalid size in a second PSSH blob
    // The CENC format allows multiple concatenated boxes, see https://www.w3.org/TR/eme-initdata-cenc/#format
    initData = new Uint8Array([
        0x00, 0x00, 0x00, 0x20,                          // size = 32
        0x70, 0x73, 0x73, 0x68,                          // 'pssh'
        0x00,                                            // version = 0
        0x00, 0x00, 0x00,                                // flags
        0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // Common SystemID
        0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
        0x00, 0x00, 0x00, 0x00,                          // datasize

        0x00, 0x00, 0x00, 0x10,                          // size = 16 (incorrect)
        0x70, 0x73, 0x73, 0x68,                          // 'pssh'
        0x00,                                            // version = 0
        0x00, 0x00, 0x00,                                // flags
        0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // Common SystemID
        0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
        0x00, 0x00, 0x00, 0x00                           // datasize

    ]);
    push_test(config.keysystem, 'cenc', initData, testnamePrefix( qualifier, config.keysystem ) + ', temporary, cenc, invalid initdata (second box has incorrect size)');

    // Valid key ID size must be at least 1 character for keyids.
    keyId = new Uint8Array(0);
    initData = stringToUint8Array(createKeyIDs(keyId));
    push_test(config.keysystem, 'keyids', initData, testnamePrefix( qualifier, config.keysystem ) + ', temporary, keyids, invalid initdata (too short key ID)');

    // Valid key ID size must be less than 512 characters for keyids.
    keyId = new Uint8Array(600);
    initData = stringToUint8Array(createKeyIDs(keyId));
    push_test(config.keysystem, 'keyids', initData, testnamePrefix( qualifier, config.keysystem ) + ', temporary, keyids, invalid initdata (too long key ID)');

    Promise.all( tests.map(function(testspec) {
        return isInitDataTypeSupported(testspec.keysystem,testspec.initDataType);
    })).then(function(results) {
        tests.filter(function(testspec, i) { return results[i]; } ).forEach(function(testspec) {
            promise_test(function(test) {
                // Create a "temporary" session for |keysystem| and call generateRequest()
                // with the provided initData. generateRequest() should fail with an
                // TypeError. Returns a promise that is resolved
                // if the error occurred and rejected otherwise.
                return navigator.requestMediaKeySystemAccess(testspec.keysystem, getSimpleConfigurationForInitDataType(testspec.initDataType)).then(function(access) {
                    return access.createMediaKeys();
                }).then(function(mediaKeys) {
                    var mediaKeySession = mediaKeys.createSession("temporary");
                    return mediaKeySession.generateRequest(testspec.initDataType, testspec.initData);
                }).then(test.step_func(function() {
                    assert_unreached('generateRequest() succeeded unexpectedly');
                }), test.step_func(function(error) {
                    assert_equals(error.name, 'TypeError');
                }));
            },testspec.testname);
        });
    });
}
