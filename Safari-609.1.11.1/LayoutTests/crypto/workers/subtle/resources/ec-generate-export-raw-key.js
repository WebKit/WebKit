importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test exporting an ECDH public key with raw format in workers.");

jsTestIsAsync = true;

var algorithmKeyGen = {
    name: "ECDH",
    namedCurve: "P-256"
};
var extractable = true;

var keyPair;
debug("Generating a key pair...");
crypto.subtle.generateKey(algorithmKeyGen, extractable, ["deriveKey", "deriveBits"]).then(function(result) {
    keyPair = result;
    debug("Exporting the public key...");
    return crypto.subtle.exportKey("raw", keyPair.publicKey);
}).then(function(result) {
    publicKey = result;

    shouldBe("publicKey.byteLength", "65");

    finishJSTest();
});
