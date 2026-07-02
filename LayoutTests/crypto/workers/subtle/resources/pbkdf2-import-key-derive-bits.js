importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test PBKDF2 deriveBits operation with an imported raw key in workers");

jsTestIsAsync = true;

var nonExtractable = false;
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var salt = asciiToUint8Array("jnOw99oO");
var expectedDerivedKey = "a0b6531f2b2c917a14238d7f01f5004c";

crypto.subtle.importKey("raw", rawKey, "PBKDF2", nonExtractable, ["deriveBits"]).then(function(baseKey) {
    return crypto.subtle.deriveBits({name: "PBKDF2", salt: salt, iterations: 100000, hash: "sha-1"}, baseKey, 128);
}).then(function(result) {
    derivedKey = result;

    shouldBe("bytesToHexString(derivedKey)", "expectedDerivedKey");

    finishJSTest();
});
