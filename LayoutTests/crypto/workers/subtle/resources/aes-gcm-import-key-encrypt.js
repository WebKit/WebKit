importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test encrypting using AES-GCM with an imported 128bit key in workers");

jsTestIsAsync = true;

var extractable = false;
var plainText = asciiToUint8Array("Hello, World!");
var aesGcmParams = {
    name: "aes-gcm",
    iv: asciiToUint8Array("jnOw99oOZFLIEPMr"),
}
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var expectedCipherText = "f9ba1161a16c9fcc726a4531c1b520925e4ba35f8b534c62f34e1f3ba1";

crypto.subtle.importKey("raw", rawKey, "aes-gcm", extractable, ["encrypt"]).then(function(key) {
    return crypto.subtle.encrypt(aesGcmParams, key, plainText);
}).then(function(result) {
    cipherText = result;

    shouldBe("bytesToHexString(cipherText)", "expectedCipherText");

    finishJSTest();
});
