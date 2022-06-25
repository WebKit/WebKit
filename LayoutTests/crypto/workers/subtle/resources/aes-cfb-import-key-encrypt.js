importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test encrypting using AES-CFB with an imported 128bit key in workers");

jsTestIsAsync = true;

var extractable = false;
var plainText = asciiToUint8Array("Hello, World!");
var aesCfbParams = {
    name: "aes-cfb-8",
    iv: asciiToUint8Array("jnOw99oOZFLIEPMr"),
}
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var expectedCipherText = "a572525a0baef88e6f5b198c6f";

crypto.subtle.importKey("raw", rawKey, "aes-cfb-8", extractable, ["encrypt"]).then(function(key) {
    return crypto.subtle.encrypt(aesCfbParams, key, plainText);
}).then(function(result) {
    cipherText = result;

    shouldBe("bytesToHexString(cipherText)", "expectedCipherText");

    finishJSTest();
});
