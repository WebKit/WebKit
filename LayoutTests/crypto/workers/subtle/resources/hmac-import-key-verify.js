importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test verification with HMAC SHA-1 using an imported key in workers");

jsTestIsAsync = true;

var extractable = false;
var text = asciiToUint8Array("Hello, World!");
var hmacImportParams = {
    name: "hmac",
    hash: "sha-1",
}
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var signature = hexStringToUint8Array("6e8e66ff128606f52b8c589196ef5e0f7ca04816");

crypto.subtle.importKey("raw", rawKey, hmacImportParams, extractable, ["sign", "verify"]).then(function(key) {
    return crypto.subtle.verify("hmac", key, signature, text);
}).then(function(result) {
    verified = result;

    shouldBeTrue("verified");

    finishJSTest();
});