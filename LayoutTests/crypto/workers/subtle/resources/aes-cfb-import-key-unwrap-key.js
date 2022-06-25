importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test unwrapping a raw key with AES-CFB using an imported key in workers");

jsTestIsAsync = true;

var extractable = true;
var expectedRawKey = "jnOw99oOZFLIEPMr";
var rawKey = asciiToUint8Array(expectedRawKey);
var aesCfbParams = {
    name: "aes-cfb-8",
    iv: asciiToUint8Array("jnOw99oOZFLIEPMr"),
}
var wrappedKey = hexStringToUint8Array("8707ee311f6e8ed157885a7fc25f0ee7");


crypto.subtle.importKey("raw", rawKey, "aes-cfb-8", extractable, ["unwrapKey"]).then(function(unwrappingKey) {
    return crypto.subtle.unwrapKey("raw", wrappedKey, unwrappingKey, aesCfbParams, {name: "aes-cbc"}, extractable, ["encrypt"]);
}).then(function(cryptoKey) {
    return crypto.subtle.exportKey("raw", cryptoKey);
}).then(function(result) {
    unwrappedKey = result;

    shouldBe("bytesToASCIIString(unwrappedKey)", "expectedRawKey");

    finishJSTest();
});
