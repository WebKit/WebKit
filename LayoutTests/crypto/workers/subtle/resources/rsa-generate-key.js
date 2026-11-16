importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test generating a RSA keypair using RSA-OAEP algorithm in workers.");
jsTestIsAsync = true;

var extractable = true;
var publicExponent = new Uint8Array([0x01, 0x00, 0x01]);

shouldReject('crypto.subtle.generateKey({name: "RSA-OAEP", modulusLength: 2048, publicExponent: publicExponent, hash: "sha-1"}, extractable, ["sign"])');
shouldReject('crypto.subtle.generateKey({name: "RSASSA-PKCS1-v1_5", modulusLength: 0, publicExponent: publicExponent, hash: "sha-1"}, extractable, ["sign", "verify"])').then(function() {
    debug("Generating a key...");
    return crypto.subtle.generateKey({name: "RSA-OAEP", modulusLength: 2048, publicExponent: publicExponent, hash: 'sha-1'}, extractable, ["decrypt", "encrypt", "wrapKey", "unwrapKey"]);
}).then(function(result) {
    keyPair = result;
    shouldBe("keyPair.toString()", "'[object Object]'");
    shouldBe("keyPair.publicKey.type", "'public'");
    shouldBe("keyPair.publicKey.extractable", "true");
    shouldBe("keyPair.publicKey.algorithm.name", "'RSA-OAEP'");
    shouldBe("keyPair.publicKey.algorithm.modulusLength", "2048");
    shouldBe("bytesToHexString(keyPair.publicKey.algorithm.publicExponent)", "'010001'");
    shouldBe("keyPair.publicKey.algorithm.hash.name", "'SHA-1'");
    shouldBe("keyPair.publicKey.usages", "['encrypt', 'wrapKey']");
    shouldBe("keyPair.privateKey.type", "'private'");
    shouldBe("keyPair.privateKey.extractable", "true");
    shouldBe("keyPair.privateKey.algorithm.name", "'RSA-OAEP'");
    shouldBe("keyPair.privateKey.algorithm.modulusLength", "2048");
    shouldBe("bytesToHexString(keyPair.privateKey.algorithm.publicExponent)", "'010001'");
    shouldBe("keyPair.privateKey.algorithm.hash.name", "'SHA-1'");
    shouldBe("keyPair.privateKey.usages", "['decrypt', 'unwrapKey']");

    finishJSTest();
});
