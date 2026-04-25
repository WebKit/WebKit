importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test exporting a RSA key pair using RSA-OAEP algorithm in workers with JWK format.");
jsTestIsAsync = true;

var algorithmKeyGen = {
    name: "RSA-OAEP",
    // RsaKeyGenParams
    modulusLength: 2048,
    publicExponent: new Uint8Array([0x01, 0x00, 0x01]),  // Equivalent to 65537
    hash: "sha-1"
};
var extractable = true;

debug("Generating a key...");
crypto.subtle.generateKey(algorithmKeyGen, extractable, ["decrypt", "encrypt", "wrapKey", "unwrapKey"]).then(function(result) {
    keyPair = result;

    // Not support format.
    shouldReject('crypto.subtle.exportKey("raw", keyPair.publicKey)');

    debug("Exporting the public key...");
    return crypto.subtle.exportKey("jwk", keyPair.publicKey);
}).then(function(result) {
    publicKey = result;

    shouldBe("publicKey.kty", "'RSA'");
    shouldBe("publicKey.key_ops", "['encrypt', 'wrapKey']");
    shouldBe("publicKey.alg", "'RSA-OAEP'");
    shouldBe("publicKey.ext", "true");
    shouldBe("Base64URL.parse(publicKey.n).byteLength", "256");
    shouldBe("bytesToHexString(Base64URL.parse(publicKey.e))", "'010001'");

    debug("Exporting the private key...");
    return crypto.subtle.exportKey("jwk", keyPair.privateKey);
}).then(function(result) {
    privateKey = result;

    shouldBe("privateKey.kty", "'RSA'");
    shouldBe("privateKey.key_ops", "['decrypt', 'unwrapKey']");
    shouldBe("privateKey.alg", "'RSA-OAEP'");
    shouldBe("privateKey.ext", "true");
    shouldBe("Base64URL.parse(privateKey.n).byteLength", "256");
    shouldBe("bytesToHexString(Base64URL.parse(privateKey.e))", "'010001'");
    shouldBeUndefined("privateKey.oth");

    finishJSTest();
});
