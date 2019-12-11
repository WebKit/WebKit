importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test generating an EC key pair with P-256 using ECDH algorithm in workers.");

jsTestIsAsync = true;

var nonExtractable = false;

debug("Generating a key pair...");
crypto.subtle.generateKey({ name: "ECDH", namedCurve: "P-256"}, nonExtractable, ["deriveKey", "deriveBits"]).then(function(result) {
    keyPair = result;
    shouldBe("keyPair.toString()", "'[object Object]'");
    shouldBe("keyPair.publicKey.type", "'public'");
    shouldBe("keyPair.publicKey.extractable", "true");
    shouldBe("keyPair.publicKey.algorithm.name", "'ECDH'");
    shouldBe("keyPair.publicKey.algorithm.namedCurve", "'P-256'");
    shouldBe("keyPair.publicKey.usages", "[ ]");
    shouldBe("keyPair.privateKey.type", "'private'");
    shouldBe("keyPair.privateKey.extractable", "false");
    shouldBe("keyPair.privateKey.algorithm.name", "'ECDH'");
    shouldBe("keyPair.privateKey.algorithm.namedCurve", "'P-256'");
    shouldBe("keyPair.privateKey.usages", "['deriveBits', 'deriveKey']");

    finishJSTest();
});