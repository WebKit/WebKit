importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test wrapping a raw key with AES-GCM using an imported key in workers");

jsTestIsAsync = true;

var extractable = true;
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var aesGcmParams = {
    name: "aes-gcm",
    iv: asciiToUint8Array("jnOw99oOZFLIEPMr"),
}
var expectWrappedKey = "dbb1327af779d0d4475e651ca516a6eeba1ec1a78bd9dd236b5ed0e1469c2ce8";

crypto.subtle.importKey("raw", rawKey, "aes-gcm", extractable, ["wrapKey"]).then(function(result) {
    wrappingKey = result;
    return crypto.subtle.importKey("raw", rawKey, "aes-cbc", extractable, ["encrypt"]);
}).then(function(result) {
    key = result;
    return crypto.subtle.wrapKey("raw", key, wrappingKey, aesGcmParams);
}).then(function(result) {
    wrappedKey = result;

    shouldBe("bytesToHexString(wrappedKey)", "expectWrappedKey");

    finishJSTest();
});
