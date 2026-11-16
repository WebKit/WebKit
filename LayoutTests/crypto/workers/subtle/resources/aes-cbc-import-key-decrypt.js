importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test decrypting using AES-CBC with an imported 128bit key in workers");

jsTestIsAsync = true;

var extractable = false;
var cipherText = hexStringToUint8Array("2ffa4618784dfd414b22c40c6330d022");
var aesCbcParams = {
    name: "aes-cbc",
    iv: asciiToUint8Array("jnOw99oOZFLIEPMr"),
}
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var expectedPlainText = "Hello, World!";

crypto.subtle.importKey("raw", rawKey, "aes-cbc", extractable, ["decrypt"]).then(function(key) {
    return crypto.subtle.decrypt(aesCbcParams, key, cipherText);
}).then(function(result) {
    plainText = result;

    shouldBe("bytesToASCIIString(plainText)", "expectedPlainText");

    finishJSTest();
});
