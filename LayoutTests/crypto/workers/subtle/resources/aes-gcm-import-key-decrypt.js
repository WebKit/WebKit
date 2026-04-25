importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test decrypting using AES-GCM with an imported 128bit key in workers");

jsTestIsAsync = true;

var extractable = false;
var cipherText = hexStringToUint8Array("f9ba1161a16c9fcc726a4531c1b520925e4ba35f8b534c62f34e1f3ba1");
var aesGcmParams = {
    name: "aes-gcm",
    iv: asciiToUint8Array("jnOw99oOZFLIEPMr"),
}
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var expectedPlainText = "Hello, World!";

crypto.subtle.importKey("raw", rawKey, "aes-gcm", extractable, ["decrypt"]).then(function(key) {
    return crypto.subtle.decrypt(aesGcmParams, key, cipherText);
}).then(function(result) {
    plainText = result;

    shouldBe("bytesToASCIIString(plainText)", "expectedPlainText");

    finishJSTest();
});
