importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test exporting an AES key using AES-CBC algorithm in workers with JWK format.");
jsTestIsAsync = true;

var extractable = true;

debug("Generating a key...");
crypto.subtle.generateKey({name: "aes-cbc", length: 128}, extractable, ["decrypt", "encrypt", "unwrapKey", "wrapKey"]).then(function(key) {
    debug("Exporting a key...");
    return crypto.subtle.exportKey("jwk", key);
}).then(function(result) {
    key = result;

    shouldBe("key.kty", "'oct'");
    shouldBe("key.key_ops", "['decrypt', 'encrypt', 'unwrapKey', 'wrapKey']");
    shouldBe("key.alg", "'A128CBC'");
    shouldBe("key.ext", "true");
    shouldBe("Base64URL.parse(key.k).byteLength", "16");

    finishJSTest();
});
