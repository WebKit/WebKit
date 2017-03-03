importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test unwrapping a raw key with AES-GCM using an imported key");

jsTestIsAsync = true;

var extractable = true;
var expectedRawKey = "jnOw99oOZFLIEPMr";
var rawKey = asciiToUint8Array(expectedRawKey);
var aesGcmParams = {
    name: "aes-gcm",
    iv: asciiToUint8Array("jnOw99oOZFLIEPMr"),
}
var wrappedKey = hexStringToUint8Array("dbb1327af779d0d4475e651ca516a6eeba1ec1a78bd9dd236b5ed0e1469c2ce8");


crypto.subtle.importKey("raw", rawKey, "aes-gcm", extractable, ["unwrapKey"]).then(function(unwrappingKey) {
    return crypto.subtle.unwrapKey("raw", wrappedKey, unwrappingKey, aesGcmParams, {name: "aes-cbc"}, extractable, ["encrypt"]);
}).then(function(cryptoKey) {
    return crypto.subtle.exportKey("raw", cryptoKey);
}).then(function(result) {
    unwrappedKey = result;

    shouldBe("bytesToASCIIString(unwrappedKey)", "expectedRawKey");

    finishJSTest();
});
