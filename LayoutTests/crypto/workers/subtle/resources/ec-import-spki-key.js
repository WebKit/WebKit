importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test importing a SPKI ECDH key in workers");

jsTestIsAsync = true;

var spkiKey = hexStringToUint8Array("3059301306072a8648ce3d020106082a8648ce3d03010703420004c3ee3a2c3380072b9b2a59fed2cada65121806e22c4f4f8a25e740fc3e54d75d86c200298e6dfc1611d185eedbdb3c2661b0eb0441f7fd57c90d08112e9ae71c");
var extractable = true;

debug("Importing a key...");
crypto.subtle.importKey("spki", spkiKey, {name: "ECDH", namedCurve: "P-256"}, extractable, [ ]).then(function(result) {
    publicKey = result;

    shouldBe("publicKey.toString()", "'[object CryptoKey]'");
    shouldBe("publicKey.type", "'public'");
    shouldBe("publicKey.extractable", "true");
    shouldBe("publicKey.algorithm.name", "'ECDH'");
    shouldBe("publicKey.algorithm.namedCurve", "'P-256'");
    shouldBe("publicKey.usages", "[ ]");

    finishJSTest();
});
