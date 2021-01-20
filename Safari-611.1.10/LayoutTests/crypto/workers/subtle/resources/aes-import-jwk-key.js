importScripts('../../../../resources/js-test-pre.js');

description("Test importing a JWK AES key using AES-CBC algorithm in workers.");
jsTestIsAsync = true;

var jwkKey = {
    kty: "oct",
    k: "YWJjZGVmZ2gxMjM0NTY3OA",
    alg: "A128CBC",
    use: "enc",
    key_ops: ["encrypt", "decrypt", "wrapKey", "unwrapKey"],
    ext: true,
};
var extractable = true;

shouldReject('crypto.subtle.importKey("jwk", {kty: "RSA"}, "aes-cbc", extractable, ["encrypt", "decrypt", "wrapKey", "unwrapKey"])');

debug("Importing a key...");
crypto.subtle.importKey("jwk", jwkKey, "aes-cbc", extractable, ["encrypt", "decrypt", "wrapKey", "unwrapKey"]).then(function(result) {
    key = result;

    shouldBe("key.type", "'secret'");
    shouldBe("key.extractable", "true");
    shouldBe("key.algorithm.name", "'AES-CBC'");
    shouldBe("key.algorithm.length", "128");
    shouldBe("key.usages", "['decrypt', 'encrypt', 'unwrapKey', 'wrapKey']");

    finishJSTest();
});
