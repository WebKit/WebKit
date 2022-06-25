importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test unwrapping a raw key with AES-CTR using an imported key in workers");

jsTestIsAsync = true;

var extractable = true;
var expectedRawKey = "jnOw99oOZFLIEPMr";
var rawKey = asciiToUint8Array(expectedRawKey);
var aesCtrParams = {
    name: "aes-ctr",
    counter: asciiToUint8Array("jnOw99oOZFLIEPMr"),
    length: 8,
}
var wrappedKey = hexStringToUint8Array("87f263f262139ba5ae4738cb37cce383");


crypto.subtle.importKey("raw", rawKey, "aes-ctr", extractable, ["unwrapKey"]).then(function(unwrappingKey) {
    return crypto.subtle.unwrapKey("raw", wrappedKey, unwrappingKey, aesCtrParams, {name: "aes-cbc"}, extractable, ["encrypt"]);
}).then(function(cryptoKey) {
    return crypto.subtle.exportKey("raw", cryptoKey);
}).then(function(result) {
    unwrappedKey = result;

    shouldBe("bytesToASCIIString(unwrappedKey)", "expectedRawKey");

    finishJSTest();
});
