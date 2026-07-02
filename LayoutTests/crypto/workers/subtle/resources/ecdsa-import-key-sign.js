importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test ECDSA signing operation with a P-256 private key in workers");

jsTestIsAsync = true;

var extractable = true;
var jwkPrivateKey = {
    kty: "EC",
    crv: "P-256",
    x: "1FSVWieTvikFkG1NOyhkUCaMbdQhxwH6aCu4Ez-sRtA",
    y: "9jmNTLqM4cjBhdAnHcNI9YQV3O8LFmo-EdZWk8ntAaI",
    d: "ppxBSov3N8_AUcisAuvmLV4yE8e_L_BLE8bZb9Z1Xjg",
};
var ecdsaParams = {
    name: "ECDSA",
    hash: "SHA-256",
}
var data = asciiToUint8Array("Hello, World!");

crypto.subtle.importKey("jwk", jwkPrivateKey, { name: "ECDSA", namedCurve: "P-256" }, extractable, ["sign"]).then(function(key) {
    return crypto.subtle.sign(ecdsaParams, key, data);
}).then(function(result) {
    signature = result;

    shouldBe("signature.byteLength", "64");

    finishJSTest();
}, failAndFinishJSTest);
