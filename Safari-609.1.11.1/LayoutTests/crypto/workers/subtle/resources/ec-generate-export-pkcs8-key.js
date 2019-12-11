importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test exporting a ECDH private key with PKCS8 format.");

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
    return crypto.subtle.exportKey("pkcs8", keyPair.privateKey);
}).then(function(result) {
    privateKey = result;

    shouldBe("privateKey.byteLength", "185");

    finishJSTest();
});
