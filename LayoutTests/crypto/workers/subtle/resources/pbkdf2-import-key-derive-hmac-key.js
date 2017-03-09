importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test deriving HMAC Keys with imported PBKDF2 base key in workers");

jsTestIsAsync = true;

var nonExtractable = false;
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var salt = asciiToUint8Array("jnOw99oO");

crypto.subtle.importKey("raw", rawKey, "PBKDF2", nonExtractable, ["deriveKey"]).then(function(baseKey) {
    return crypto.subtle.deriveKey({name: "PBKDF2", salt: salt, iterations: 100000, hash: "sha-1"}, baseKey, {name: "hmac", hash: "sha-1"}, nonExtractable, ['sign', 'verify']);
}).then(function(result) {
    derivedKey = result;

    shouldBe("derivedKey.type", "'secret'");
    shouldBe("derivedKey.extractable", "false");
    shouldBe("derivedKey.algorithm.name", "'HMAC'");
    shouldBe("derivedKey.algorithm.length", "512");
    shouldBe("derivedKey.algorithm.hash.name", "'SHA-1'");
    shouldBe("derivedKey.usages", "['sign', 'verify']");

    finishJSTest();
});
