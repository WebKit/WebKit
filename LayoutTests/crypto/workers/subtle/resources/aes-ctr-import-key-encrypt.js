importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test encrypting using AES-CTR with an imported 128bit key in workers");

jsTestIsAsync = true;

var extractable = false;
var plainText = asciiToUint8Array("Hello, World!Hello, World!Hello, World!Hello, World!");
var aesCtrParams = {
    name: "aes-ctr",
    counter: asciiToUint8Array("jnOw99oOZFLIEPMr"),
    length: 8,
}
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var expectedCipherText = "a5f940e93406d4bd9b7318e653d4cb9d1af497f52fcbb659a038e711e8bd61fb4863931d25911e2e9ff30cf37ec27dd813a62830";

crypto.subtle.importKey("raw", rawKey, "aes-ctr", extractable, ["encrypt"]).then(function(key) {
    return crypto.subtle.encrypt(aesCtrParams, key, plainText);
}).then(function(result) {
    cipherText = result;

    shouldBe("bytesToHexString(cipherText)", "expectedCipherText");

    finishJSTest();
});
