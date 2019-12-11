importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test unwrapping a raw key with AES-KW using an imported key in workers");

jsTestIsAsync = true;

var extractable = true;
var rawKeyASCII = "jnOw99oOZFLIEPMr";
var rawKey = asciiToUint8Array(rawKeyASCII);
var wrappedKey = hexStringToUint8Array("d64787ab3e048dbdc30bb62781c9f18e58ad7dbfc64aab16");

crypto.subtle.importKey("raw", rawKey, "aes-kw", extractable, ["wrapKey", "unwrapKey"]).then(function(unwrappingKey) {
    return crypto.subtle.unwrapKey("raw", wrappedKey, unwrappingKey, "AES-KW", "AES-CBC", extractable, ["encrypt", "decrypt"]);
}).then(function(cryptoKey) {
    return crypto.subtle.exportKey("raw", cryptoKey);
}).then(function(result) {
    unwrappedKey = result;

    shouldBe("bytesToASCIIString(unwrappedKey)", "rawKeyASCII");

    finishJSTest();
});
