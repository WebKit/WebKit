importScripts('../../../../resources/js-test-pre.js');

description("Test generating an AES key using AES-CBC algorithm in workers.");
jsTestIsAsync = true;

const jwkKey = { kty: "oct", k: "YWJjZGVmZ2gxMjM0NTY3OA", alg: "A128CBC", use: "enc", key_ops: ['decrypt', 'encrypt', 'unwrapKey', 'wrapKey'], ext: true };

debug("Importing a key...");
crypto.subtle.importKey("jwk", jwkKey, "aes-cbc", true, ["encrypt", "decrypt", "wrapKey", "unwrapKey"]).then(function(key) {
    const openRequest = indexedDB.open("crypto_subtle");
    openRequest.onupgradeneeded = function(event) {
        event.target.result.createObjectStore("aes-indexeddb");
    };
    openRequest.onerror = function(event) {
        testFailed("Could not open database: " + event.target.error.name);
        finishJSTest();
    };
    openRequest.onsuccess = function(event) {
        db = event.target.result;
        storeKey();
    };

    function storeKey() {
        const objectStore = db.transaction("aes-indexeddb", "readwrite").objectStore("aes-indexeddb");
        const req = objectStore.put(key, "myKey");
        req.onerror = function(event) {
            testFailed("Could not put a key into database: " + event.target.error.name);
            finishJSTest();
        };
        req.onsuccess = function() { readKey(); }
    }

    function readKey() {
        const objectStore = db.transaction("aes-indexeddb").objectStore("aes-indexeddb");
        const req = objectStore.get("myKey");
        req.onerror = function(event) {
            testFailed("Could not get a key from database: " + event.target.error.name);
            finishJSTest();
        };
        req.onsuccess = function(event) {
            retrievedKey = event.target.result;
            shouldBe("retrievedKey.type", "'secret'");
            shouldBe("retrievedKey.extractable", "true");
            shouldBe("retrievedKey.algorithm.name", "'AES-CBC'");
            shouldBe("retrievedKey.algorithm.length", "128");
            shouldBe("retrievedKey.usages", "['decrypt', 'encrypt', 'unwrapKey', 'wrapKey']");

            finishJSTest();
        }
    }
});
