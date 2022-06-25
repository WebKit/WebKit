importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test importing a SPKI RSA key in workers");

jsTestIsAsync = true;

var spkiKey = hexStringToUint8Array("30820122300d06092a864886f70d01010105000382010f003082010a0282010100d61051c4920d4c7c04a33beabb2412bedc4173fdbe82c7db80f0052d00abd32e01656d2105abef4fa5417bb868bb77780c08d20cfe9e2d767ee952970941398f8bea59a966772b1d89f19693cfff5faba22ba39ef7982ac46257180d557d3cbf3f7e65dd835d1d351e679a841b904a3cf24a0323aac0bb89705777507c76f57663b6d846e995a63057ceb48053ddf9282a366d6dc6cbdceb2ebcb3b374c7dc49da160cba8a3656211b8597a39ef9b8cc44d0c5735f870679ffcb1cb9321bbfbc307844d3174ef5dc144a951071340f4d156bead21fd3d9bd4641afa5851512264b4eed228b5d6324de428b00cbb89a1a88cb138d59de0b4ad75f2196e0e7ba890203010001");
var extractable = true;

shouldReject('crypto.subtle.importKey("spki", spkiKey, {name: "RSA-OAEP", hash: "sha-1"}, extractable, ["decrypt", "unwrapKey"])');

debug("Importing a key...");
crypto.subtle.importKey("spki", spkiKey, {name: "RSA-OAEP", hash: "sha-1"}, extractable, ["encrypt", "wrapKey"]).then(function(result) {
    publicKey = result;

    shouldBe("publicKey.toString()", "'[object CryptoKey]'");
    shouldBe("publicKey.type", "'public'");
    shouldBe("publicKey.extractable", "true");
    shouldBe("publicKey.algorithm.name", "'RSA-OAEP'");
    shouldBe("publicKey.algorithm.modulusLength", "2048");
    shouldBe("bytesToHexString(publicKey.algorithm.publicExponent)", "'010001'");
    shouldBe("publicKey.algorithm.hash.name", "'SHA-1'");
    shouldBe("publicKey.usages", "['encrypt', 'wrapKey']");

    finishJSTest();
});
