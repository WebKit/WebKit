importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test encrypting using AES-CBC with an imported 128bit key in workers");

jsTestIsAsync = true;

var extractable = false;
var plainText = asciiToUint8Array("Hello, World!");
var aesCbcParams = {
    name: "aes-cbc",
    iv: asciiToUint8Array("jnOw99oOZFLIEPMr"),
}
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var expectedCipherText = "2ffa4618784dfd414b22c40c6330d022";

crypto.subtle.importKey("raw", rawKey, "aes-cbc", extractable, ["encrypt"]).then(function(key) {
    return crypto.subtle.encrypt(aesCbcParams, key, plainText);
}).then(function(result) {
    cipherText = result;

    shouldBe("bytesToHexString(cipherText)", "expectedCipherText");

    finishJSTest();
});