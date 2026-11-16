importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test exporting an ECDH key pair with JWK format in workers.");

jsTestIsAsync = true;

var algorithmKeyGen = {
    name: "ECDH",
    namedCurve: "P-384"
};
var extractable = true;

var keyPair;
debug("Generating a key pair...");
crypto.subtle.generateKey(algorithmKeyGen, extractable, ["deriveKey", "deriveBits"]).then(function(result) {
    keyPair = result;
    debug("Exporting the public key...");
    return crypto.subtle.exportKey("jwk", keyPair.publicKey);
}).then(function(result) {
    publicKey = result;

    shouldBe("publicKey.kty", "'EC'");
    shouldBe("publicKey.crv", "'P-384'");
    shouldBe("Base64URL.parse(publicKey.x).byteLength", "48");
    shouldBe("Base64URL.parse(publicKey.y).byteLength", "48");
    shouldBeUndefined("publicKey.d");
    shouldBe("publicKey.key_ops", "[ ]");
    shouldBe("publicKey.ext", "true");

    debug("Exporting the private key...");
    return crypto.subtle.exportKey("jwk", keyPair.privateKey);
}).then(function(result) {
    privateKey = result;

    shouldBe("privateKey.kty", "'EC'");
    shouldBe("privateKey.crv", "'P-384'");
    shouldBe("Base64URL.parse(privateKey.x).byteLength", "48");
    shouldBe("Base64URL.parse(privateKey.y).byteLength", "48");
    shouldBe("Base64URL.parse(privateKey.d).byteLength", "48");
    shouldBe("privateKey.key_ops", "['deriveBits', 'deriveKey']");
    shouldBe("privateKey.ext", "true");

    finishJSTest();
});
