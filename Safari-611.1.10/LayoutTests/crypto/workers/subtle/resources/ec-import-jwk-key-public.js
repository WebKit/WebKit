importScripts('../../../../resources/js-test-pre.js');
importScripts("../../../resources/common.js");

description("Test importing a JWK ECDH public key in workers");

jsTestIsAsync = true;

var jwkKey = {
    kty: "EC",
    use: "enc",
    ext: true,
    crv: "P-384",
    x: "1bHwFrsaPRjYq-zFOyLXK8Ugv3EqbVF075ct7ouTl_pwyhjeBu03JHjKTsyVbNWK",
    y: "1bHwFrsaPRjYq-zFOyLXK8Ugv3EqbVF075ct7ouTl_pwyhjeBu03JHjKTsyVbNWK",
};
var extractable = true;

debug("Importing a key...");
crypto.subtle.importKey("jwk", jwkKey, { name: "ECDH", namedCurve: "P-384" }, extractable, [ ]).then(function(result) {
    publicKey = result;

    shouldBe("publicKey.toString()", "'[object CryptoKey]'");
    shouldBe("publicKey.type", "'public'");
    shouldBe("publicKey.extractable", "true");
    shouldBe("publicKey.algorithm.name", "'ECDH'");
    shouldBe("publicKey.algorithm.namedCurve", "'P-384'");
    shouldBe("publicKey.usages", "[ ]");

    finishJSTest();
});
