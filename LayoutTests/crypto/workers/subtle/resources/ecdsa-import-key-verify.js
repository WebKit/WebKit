importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test ECDSA verifying operation with a P-256 public key in workers");

jsTestIsAsync = true;

var extractable = true;
var jwkPublicKey = {
    kty: "EC",
    crv: "P-256",
    x: "1FSVWieTvikFkG1NOyhkUCaMbdQhxwH6aCu4Ez-sRtA",
    y: "9jmNTLqM4cjBhdAnHcNI9YQV3O8LFmo-EdZWk8ntAaI",
};
var ecdsaParams = {
    name: "ECDSA",
    hash: "SHA-256",
}
var data = asciiToUint8Array("Hello, World!");
var signature = hexStringToUint8Array("d60737267c717deb1f3547c85d3f49b167fb3b4fd7ed7d974c2adc4f89171f9884a2637d5d5a7c5d475dc13d37522b958d5bf333a06e9ad445e831f220900a1b");

crypto.subtle.importKey("jwk", jwkPublicKey, { name: "ECDSA", namedCurve: "P-256" }, extractable, ["verify"]).then(function(key) {
    return crypto.subtle.verify(ecdsaParams, key, signature, data);
}).then(function(result) {
    verified = result;

    shouldBeTrue("verified");

    finishJSTest();
}, failAndFinishJSTest);
