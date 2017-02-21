importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test wrapping a raw key with AES-CFB using an imported key in workers");

jsTestIsAsync = true;

var extractable = true;
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var aesCfbParams = {
    name: "aes-cfb-8",
    iv: asciiToUint8Array("jnOw99oOZFLIEPMr"),
}
var expectWrappedKey = "8707ee311f6e8ed157885a7fc25f0ee7";

crypto.subtle.importKey("raw", rawKey, "aes-cfb-8", extractable, ["wrapKey"]).then(function(result) {
    wrappingKey = result;
    return crypto.subtle.importKey("raw", rawKey, "aes-cbc", extractable, ["encrypt"]);
}).then(function(result) {
    key = result;
    return crypto.subtle.wrapKey("raw", key, wrappingKey, aesCfbParams);
}).then(function(result) {
    wrappedKey = result;

    shouldBe("bytesToHexString(wrappedKey)", "expectWrappedKey");

    finishJSTest();
});
