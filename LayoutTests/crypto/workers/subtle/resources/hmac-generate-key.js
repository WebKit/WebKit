importScripts('../../../../resources/js-test-pre.js');

description("Test generating an HMAC key using SHA-1 algorithm in workers.");
jsTestIsAsync = true;

var extractable = true;

shouldReject('crypto.subtle.generateKey({name: "hmac", hash: "sha-1"}, extractable, ["encrypt"])');
shouldReject('crypto.subtle.generateKey({name: "hmac", hash: "sha-1", length: 0}, extractable, ["sign", "verify"])');

debug("Generating a key...");
crypto.subtle.generateKey({name: "hmac", hash: "sha-1"}, extractable, ["sign", "verify"]).then(function(result) {
    key = result;

    shouldBe("key.type", "'secret'");
    shouldBe("key.extractable", "true");
    shouldBe("key.algorithm.name", "'HMAC'");
    shouldBe("key.algorithm.length", "512");
    shouldBe("key.algorithm.hash.name", "'SHA-1'");
    shouldBe("key.usages", '["sign", "verify"]');

    finishJSTest();
});
