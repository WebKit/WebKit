importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test ECDH deriveBits operation with imported base key in workers");

jsTestIsAsync = true;

var extractable = true;
var jwkPrivateKey = {
    kty: "EC",
    crv: "P-256",
    x: "1FSVWieTvikFkG1NOyhkUCaMbdQhxwH6aCu4Ez-sRtA",
    y: "9jmNTLqM4cjBhdAnHcNI9YQV3O8LFmo-EdZWk8ntAaI",
    d: "ppxBSov3N8_AUcisAuvmLV4yE8e_L_BLE8bZb9Z1Xjg",
};
var jwkPublicKey = {
    kty: "EC",
    crv: "P-256",
    x: "1FSVWieTvikFkG1NOyhkUCaMbdQhxwH6aCu4Ez-sRtA",
    y: "9jmNTLqM4cjBhdAnHcNI9YQV3O8LFmo-EdZWk8ntAaI",
};
var expectedDerivedKey = "40bf0c0a56f75ca587ea4f6729d7bf9a30c5f4a0d47eea13fdf9da8f0b53b85e";

crypto.subtle.importKey("jwk", jwkPrivateKey, { name: "ECDH", namedCurve: "P-256" }, extractable, ["deriveBits"]).then(function(result) {
    privateKey = result;
    return crypto.subtle.importKey("jwk", jwkPublicKey, { name: "ECDH", namedCurve: "P-256" }, extractable, [ ]);
}).then(function(result) {
    publicKey = result;
    return crypto.subtle.deriveBits({ name:"ECDH", public:publicKey }, privateKey, null);
}).then(function(result) {
    derivedKey = result;

    shouldBe("bytesToHexString(derivedKey)", "expectedDerivedKey");

    finishJSTest();
}, failAndFinishJSTest);
