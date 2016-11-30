importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test wrapping a raw key with AES-KW using an imported key in workers");

jsTestIsAsync = true;

var extractable = true;
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var expectWrappedKey = "d64787ab3e048dbdc30bb62781c9f18e58ad7dbfc64aab16";

crypto.subtle.importKey("raw", rawKey, "aes-kw", extractable, ["wrapKey", "unwrapKey"]).then(function(result) {
    wrappingKey = result;
    return crypto.subtle.importKey("raw", rawKey, "aes-cbc", extractable, ["encrypt", "decrypt"]);
}).then(function(result) {
    key = result;
    return crypto.subtle.wrapKey("raw", key, wrappingKey, "AES-KW");
}).then(function(result) {
    wrappedKey = result;

    shouldBe("bytesToHexString(wrappedKey)", "expectWrappedKey");
    shouldReject('crypto.subtle.wrapKey("jwk", key, wrappingKey, "AES-KW")').then(finishJSTest);
});
