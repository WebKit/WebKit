importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test importing a raw HMAC key with SHA-1 in workers.");
jsTestIsAsync = true;

var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var extractable = true;

shouldReject('crypto.subtle.importKey("raw", asciiToUint8Array(""), {name: "hmac", hash: "sha-1"}, extractable, ["sign", "verify"])');

debug("Importing a key...");
crypto.subtle.importKey("raw", rawKey, {name: "hmac", hash: "sha-1"}, extractable, ["sign", "verify"]).then(function(result) {
    key = result;

    shouldBe("key.type", "'secret'");
    shouldBe("key.extractable", "true");
    shouldBe("key.algorithm.name", "'HMAC'");
    shouldBe("key.algorithm.length", "128");
    shouldBe("key.algorithm.hash.name", "'SHA-1'");
    shouldBe("key.usages", "['sign', 'verify']");

    finishJSTest();
});
