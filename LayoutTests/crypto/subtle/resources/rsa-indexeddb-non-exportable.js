description("Test storing a private RSA key in IndexedDB, and retrieving it.");

jsTestIsAsync = true;

var privateKeyJSON = {
    kty: "RSA",
    alg: "RS256",
    n: "rcCUCv7Oc1HVam1DIhCzqknThWawOp8QLk8Ziy2p10ByjQFCajoFiyuAWl-R1WXZaf4xitLRracT9agpzIzc-MbLSHIGgWQGO21lGiImy5ftZ-D8bHAqRz2y15pzD4c4CEou7XSSLDoRnR0QG5MsDhD6s2gV9mwHkrtkCxtMWdBi-77as8wGmlNRldcOSgZDLK8UnCSgA1OguZ989bFyc8tOOEIb0xUSfPSz3LPSCnyYz68aDjmKVeNH-ig857OScyWbGyEy3Biw64qun3juUlNWsJ3zngkOdteYWytx5Qr4XKNs6R-Myyq72KUp02mJDZiiyiglxML_i3-_CeecCw",
    e: "AQAB",
    d: "eNLS37aCz7RXSNPD_DtLBJ6j5T8cSxdzRBCjPaI6WcGqJp16lq3UTwuoDLAqlA9oGYm238dsIWpuucP_lQtbWe-7SpxoI6_vmYGf7YVUHv1-DF9qiOmSrMmdxMnVOzYXY8RaT6thPjn_J5cfLV2xI_LwsrMtmpdSyNlgX0zTUhwtuahgAKMEChYjH2EnjHdHw6sY2-wApdcQI7ULE0oo5RzbQZpmuhcN9hiBc0L3hhF0qo50mbl02_65_GQ7DpVkXBxNgRBLzlPabmzzG2oAhfefLgYmSC1opaCkXE6vRWQNWNL45RZNZFYM3uoJghOMqGeocM0BpjdChHrPOlFvSQ",
    p: "4miTuAjKMeH5uJ5KB397QUwhbkYEgSbcA2mifmSkvE2018gb55qkBHK1eVryf1_m43LNlc6O_ak6gfzdZIZvS5NCGjPl0q09plUpu8qFOSspBwA67qGH76lFlZLn_d4yglS7wfLru4_5Ys8qLLs-DqVLviwposOnyyWqwM5AXp0",
    q: "xHYrzkivtmnz_sGchnWGc0q-pDOkKicptRpv2pMFIIXxnFX5aMeEXIZjVujXtwUy1UlFIN2GZJSvy5KJ79mu_XyNnFHMzedH-A3ee3u8h1UUrZF-vUu1_e4U_x67NN1dedzUSKynN7pFl3OkuShMBWGV-cwzOPdcVAfVuZlxUMc",
    dp: "fBzDzYDUBmBQGop7Hn0dvf_T27V6RqpctWo074CQZcFbP2atFVtKSj3viWT3xid2VHzcgiDHdfpM3nEVlEO1wwIonGCSvdjGEOZiiFVOjrZAOVxA8guOjyyFvqbXke06VwPIIVvfKeSU2zuhbP__1tt6F_fxow4Kb2xonGT0GGk",
    dq: "jmE2DiIPdhwDgLXAQpIaBqQ81bO3XfVT_LRULAwwwwlPuQV148H04zlh9TJ6Y2GZHYokV1U0eOBpJxfkb7dLYtpJpuiBjRf4yIUEoGlkkI_QlJnFSFr-YjGRdfNHqWBkxlSMZL770R9mIATndGkH7z5x-r9KwBZFC4FCG2hg_zE",
    qi: "YCX_pLwbMBA1ThVH0WcwmnytqNcrMCEwTm7ByA2eU6nWbQrULvf7m9_kzfLUcjsnpAVlBQG5JMXMy0Sq4ptwbywsa5-G8KAOOOR2L3v4hC-Eys9ftgFM_3i0o40eeQH4b3haPbntrIeMg8IzlOuVYKf9-2QuKDoWeRdd7NsdxTk"
};

crypto.subtle.importKey("jwk", privateKeyJSON, { name: 'RSASSA-PKCS1-v1_5', hash: "sha-256" }, false, ["sign"]).then(function(key) {
    var openRequest = indexedDB.open("crypto_subtle");
    openRequest.onupgradeneeded = function(event) {
        var objectStore = event.target.result.createObjectStore("rsa-indexeddb");
    }
    openRequest.onerror = function(event) {
        testFailed("Could not open database: " + event.target.error.name);
        finishJSTest();
    }
    openRequest.onsuccess = function(event) {
        db = event.target.result;
        storeKey();
    }

    function storeKey() {
        var objectStore = db.transaction("rsa-indexeddb", "readwrite").objectStore("rsa-indexeddb");
        var req = objectStore.put(key, "mykey");
        req.onerror = function(event) {
            testFailed("Could not put a key into database: " + event.target.error.name);
            finishJSTest();
        }
        req.onsuccess = function(event) { readKey(); }
    }

    function readKey() {
        var objectStore = db.transaction("rsa-indexeddb").objectStore("rsa-indexeddb");
        var req = objectStore.get("mykey");
        req.onerror = function(event) {
            testFailed("Could not get a key from database: " + event.target.error.name);
            finishJSTest();
        }
        req.onsuccess = function(event) {
            window.retrievedKey = event.target.result;
            shouldBe("retrievedKey.type", "'private'");
            shouldBe("retrievedKey.extractable", "false");
            shouldBe("retrievedKey.algorithm.name", "'RSASSA-PKCS1-v1_5'");
            shouldBe("retrievedKey.algorithm.modulusLength", "2048");
            shouldBe("bytesToHexString(retrievedKey.algorithm.publicExponent)", "'010001'");
            shouldBe("retrievedKey.algorithm.hash.name", "'SHA-256'");
            shouldBe("retrievedKey.usages", '["sign"]');

            finishJSTest();
        }
    }
});
