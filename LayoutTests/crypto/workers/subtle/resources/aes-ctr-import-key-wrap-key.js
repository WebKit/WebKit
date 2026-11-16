importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test wrapping a raw key with AES-CTR using an imported key in workers");

jsTestIsAsync = true;

var extractable = true;
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var aesCtrParams = {
    name: "aes-ctr",
    counter: asciiToUint8Array("jnOw99oOZFLIEPMr"),
    length: 8,
}
var expectWrappedKey = "87f263f262139ba5ae4738cb37cce383";

crypto.subtle.importKey("raw", rawKey, "aes-ctr", extractable, ["wrapKey"]).then(function(result) {
    wrappingKey = result;
    return crypto.subtle.importKey("raw", rawKey, "aes-cbc", extractable, ["encrypt"]);
}).then(function(result) {
    key = result;
    return crypto.subtle.wrapKey("raw", key, wrappingKey, aesCtrParams);
}).then(function(result) {
    wrappedKey = result;

    shouldBe("bytesToHexString(wrappedKey)", "expectWrappedKey");

    finishJSTest();
});
