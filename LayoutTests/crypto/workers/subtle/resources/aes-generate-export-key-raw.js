importScripts('../../../../resources/js-test-pre.js');

description("Test exporting an AES key using AES-CBC algorithm in workers with raw format.");
jsTestIsAsync = true;

var extractable = true;

debug("Generating a key...");
crypto.subtle.generateKey({name: "aes-cbc", length: 128}, extractable, ["decrypt", "encrypt", "unwrapKey", "wrapKey"]).then(function(result) {
    key = result;

    // Not support format.
    shouldReject('crypto.subtle.exportKey("spki", key)');
    shouldReject('crypto.subtle.exportKey("pkcs8", key)');

    debug("Generating a key...");
    return crypto.subtle.generateKey({name: "aes-cbc", length: 128}, extractable, ["decrypt", "encrypt", "unwrapKey", "wrapKey"]);
}).then(function(key) {
    debug("Exporting a key...");
    return crypto.subtle.exportKey("raw", key);
}).then(function(result) {
    key = result;
    shouldBe("key.byteLength", "16");

    finishJSTest();
});
