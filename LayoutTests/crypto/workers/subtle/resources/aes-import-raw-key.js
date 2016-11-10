importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test importing a raw AES key using AES-CBC algorithm in workers.");
jsTestIsAsync = true;

var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var extractable = true;

shouldReject('crypto.subtle.importKey("raw", asciiToUint8Array("jnOw97"), "aes-cbc", extractable, ["encrypt", "decrypt", "wrapKey", "unwrapKey"])');

debug("Importing a key...");
crypto.subtle.importKey("raw", rawKey, "aes-cbc", extractable, ["encrypt", "decrypt", "wrapKey", "unwrapKey"]).then(function(result) {
    key = result;

    shouldBe("key.type", "'secret'");
    shouldBe("key.extractable", "true");
    shouldBe("key.algorithm.name", "'AES-CBC'");
    shouldBe("key.algorithm.length", "128");
    shouldBe("key.usages", "['decrypt', 'encrypt', 'unwrapKey', 'wrapKey']");

    finishJSTest();
});
