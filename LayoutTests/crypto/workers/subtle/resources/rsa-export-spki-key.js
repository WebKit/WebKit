importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test exporting a RSA-OAEP public key with SPKI format in workers.");

jsTestIsAsync = true;

var algorithmKeyGen = {
    name: "RSA-OAEP",
    // RsaKeyGenParams
    modulusLength: 2048,
    publicExponent: new Uint8Array([0x01, 0x00, 0x01]),  // Equivalent to 65537
    hash: "sha-1"
};
var extractable = true;

var keyPair;
debug("Generating a key pair...");
crypto.subtle.generateKey(algorithmKeyGen, extractable, ["decrypt", "encrypt", "wrapKey", "unwrapKey"]).then(function(result) {
    keyPair = result;
    debug("Exporting the public key...");
    return crypto.subtle.exportKey("spki", keyPair.publicKey);
}).then(function(result) {
    publicKey = result;

    shouldBe("publicKey.byteLength", "294");

    finishJSTest();
});