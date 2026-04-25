importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test HKDF deriveBits operation with an imported raw key in workers");

jsTestIsAsync = true;

var nonExtractable = false;
var rawKey = hexStringToUint8Array("0b0b0b0b0b0b0b0b0b0b0b");
var info = hexStringToUint8Array("f0f1f2f3f4f5f6f7f8f9");
var salt = hexStringToUint8Array("000102030405060708090a0b0c");
var expectedDerivedKey = "085a01ea1b10f36933068b56efa5ad81a4f14b822f5b091568a9cdd4f155fda2c22e422478d305f3f896";

crypto.subtle.importKey("raw", rawKey, "HKDF", nonExtractable, ["deriveBits"]).then(function(baseKey) {
    return crypto.subtle.deriveBits({name: "HKDF", salt: salt, info: info, hash: "sha-1"}, baseKey, 336);
}).then(function(result) {
    derivedKey = result;

    shouldBe("bytesToHexString(derivedKey)", "expectedDerivedKey");

    finishJSTest();
});
