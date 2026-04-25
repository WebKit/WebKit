importScripts('../../../../resources/js-test-pre.js');

description("Test importing a JWK HMAC key with SHA-1 in workers.");
jsTestIsAsync = true;

var jwkKey = {
    kty: "oct",
    k: "YWJjZGVmZ2gxMjM0NTY3OA",
    alg: "HS1",
    use: "sig",
    key_ops: ["sign", "verify"],
    ext: true,
};
var extractable = true;

shouldReject('crypto.subtle.importKey("jwk", {kty: "RSA"}, {name: "hmac", hash: "sha-1"}, extractable, ["sign", "verify"])');

debug("Importing a key...");
crypto.subtle.importKey("jwk", jwkKey, {name: "hmac", hash: "sha-1"}, extractable, ["sign", "verify"]).then(function(result) {
    key = result;

    shouldBe("key.type", "'secret'");
    shouldBe("key.extractable", "true");
    shouldBe("key.algorithm.name", "'HMAC'");
    shouldBe("key.algorithm.length", "128");
    shouldBe("key.algorithm.hash.name", "'SHA-1'");
    shouldBe("key.usages", "['sign', 'verify']");

    finishJSTest();
});
