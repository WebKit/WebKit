importScripts('../../../../resources/js-test-pre.js');

description("Test generating an AES key using AES-CBC algorithm in workers.");
jsTestIsAsync = true;

var extractable = true;

shouldReject('crypto.subtle.generateKey({name: "aes-cbc", length: 128}, extractable, ["sign"])');
shouldReject('crypto.subtle.generateKey({name: "aes-cbc", length: 111}, extractable, ["encrypt"])');

debug("Generating a key...");
crypto.subtle.generateKey({name: "aes-cbc", length: 128}, extractable, ["decrypt", "encrypt", "unwrapKey", "wrapKey"]).then(function(result) {
    key = result;

    shouldBe("key.type", "'secret'");
    shouldBe("key.extractable", "true");
    shouldBe("key.algorithm.name", "'AES-CBC'");
    shouldBe("key.algorithm.length", "128");
    shouldBe("key.usages", "['decrypt', 'encrypt', 'unwrapKey', 'wrapKey']");

    finishJSTest();
});
