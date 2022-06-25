importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test encrypting using RSAES-PKCS1-v1_5 with an imported key in workers");

jsTestIsAsync = true;

var extractable = false;
var plainText = asciiToUint8Array("Hello, World!");
var jwkKey = {
    kty: "RSA",
    alg: "RSA1_5",
    use: "enc",
    key_ops: ["encrypt"],
    ext: true,
    n: "rcCUCv7Oc1HVam1DIhCzqknThWawOp8QLk8Ziy2p10ByjQFCajoFiyuAWl-R1WXZaf4xitLRracT9agpzIzc-MbLSHIGgWQGO21lGiImy5ftZ-D8bHAqRz2y15pzD4c4CEou7XSSLDoRnR0QG5MsDhD6s2gV9mwHkrtkCxtMWdBi-77as8wGmlNRldcOSgZDLK8UnCSgA1OguZ989bFyc8tOOEIb0xUSfPSz3LPSCnyYz68aDjmKVeNH-ig857OScyWbGyEy3Biw64qun3juUlNWsJ3zngkOdteYWytx5Qr4XKNs6R-Myyq72KUp02mJDZiiyiglxML_i3-_CeecCw",
    e: "AQAB"
};

crypto.subtle.importKey("jwk", jwkKey, "RSAES-PKCS1-v1_5", extractable, ["encrypt"]).then(function(key) {
    return crypto.subtle.encrypt("RSAES-PKCS1-v1_5", key, plainText);
}).then(function(result) {
    cipherText = result;

    shouldBe("cipherText.byteLength", "256");

    finishJSTest();
});
