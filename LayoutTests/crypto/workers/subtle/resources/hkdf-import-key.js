importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test importing a HKDF raw key in workers");

jsTestIsAsync = true;

var nonExtractable = false;
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");

crypto.subtle.importKey("raw", rawKey, { name: "HKDF" }, nonExtractable, ["deriveKey", "deriveBits"]).then(function(result) {
    publicKey = result;

    shouldBe("publicKey.toString()", "'[object CryptoKey]'");
    shouldBe("publicKey.type", "'secret'");
    shouldBe("publicKey.extractable", "false");
    shouldBe("publicKey.algorithm.name", "'HKDF'");
    shouldBe("publicKey.usages", "['deriveBits', 'deriveKey']");

    finishJSTest();
})
