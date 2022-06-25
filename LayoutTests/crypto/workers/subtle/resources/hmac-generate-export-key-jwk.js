importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test exporting a HMAC key in workers with JWK format.");
jsTestIsAsync = true;

var extractable = true;

debug("Generating a key...");
crypto.subtle.generateKey({name: "hmac", hash: "sha-1"}, extractable, ["sign", "verify"]).then(function(key) {
    debug("Exporting a key...");
    return crypto.subtle.exportKey("jwk", key);
}).then(function(result) {
    key = result;

    shouldBe("key.kty", "'oct'");
    shouldBe("key.key_ops", "['sign', 'verify']");
    shouldBe("key.alg", "'HS1'");
    shouldBe("key.ext", "true");
    shouldBe("Base64URL.parse(key.k).byteLength", "64");

    finishJSTest();
});
