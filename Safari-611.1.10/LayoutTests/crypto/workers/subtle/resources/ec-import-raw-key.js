importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test importing an ECDH raw key in workers");

jsTestIsAsync = true;

var rawKey = hexStringToUint8Array("04dc08d9bf603333eab1ad29cef41a6203ab6ecce03c5a4a9bf5771a3fb9f971d89a09664bfb87c61199b3453220eadec714c49ad1e24bf5d7ec5bddeca6420893");
var extractable = true;

debug("Importing a key...");
crypto.subtle.importKey("raw", rawKey, { name: "ECDH", namedCurve: "P-256" }, extractable, [ ]).then(function(result) {
    publicKey = result;

    shouldBe("publicKey.toString()", "'[object CryptoKey]'");
    shouldBe("publicKey.type", "'public'");
    shouldBe("publicKey.extractable", "true");
    shouldBe("publicKey.algorithm.name", "'ECDH'");
    shouldBe("publicKey.algorithm.namedCurve", "'P-256'");
    shouldBe("publicKey.usages", "[ ]");

    finishJSTest();
});
