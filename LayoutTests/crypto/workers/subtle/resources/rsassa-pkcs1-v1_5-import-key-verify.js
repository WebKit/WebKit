importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test verification with RSASSA-PKCS1-v1_5 SHA-1 using an imported key in workers");

jsTestIsAsync = true;

var extractable = false;
var text = asciiToUint8Array("Hello, World!");
var rsaImportParams = {
    name: "RSASSA-PKCS1-v1_5",
    hash: "SHA-1",
}
var jwkKey = {
    kty: "RSA",
    alg: "RS1",
    use: "sig",
    key_ops: ["verify"],
    ext: true,
    n: "rcCUCv7Oc1HVam1DIhCzqknThWawOp8QLk8Ziy2p10ByjQFCajoFiyuAWl-R1WXZaf4xitLRracT9agpzIzc-MbLSHIGgWQGO21lGiImy5ftZ-D8bHAqRz2y15pzD4c4CEou7XSSLDoRnR0QG5MsDhD6s2gV9mwHkrtkCxtMWdBi-77as8wGmlNRldcOSgZDLK8UnCSgA1OguZ989bFyc8tOOEIb0xUSfPSz3LPSCnyYz68aDjmKVeNH-ig857OScyWbGyEy3Biw64qun3juUlNWsJ3zngkOdteYWytx5Qr4XKNs6R-Myyq72KUp02mJDZiiyiglxML_i3-_CeecCw",
    e: "AQAB",
};
var signature = hexStringToUint8Array("08b83a4a58aff1db76529fe97f6dc2b58914a56f08ebc73aebf740680fc524e721c30842759acacab8acdf6e31e7cc83dbe8a48ace80b6a6558a9df892e4c2a9276c154cd605e632c8b857855a064aea68550b660a15044a0917026e494a71e4fee99fa57941268545f338c8601baa9c4f2116f0eb5432cfee5e3ed9978876fed201647921e599101930a99a2b645a9a879bce8c24592729f1f62a3b83fe21475871273cdbbe8efa7be314cabd087fbe1931ddf3fb86cbff5176a27554726b54c43208200725c6ad71d335ee5e09f3bbff7164ddf5bc857a41c5961e6f9581459db0fd0f09ee251d63b7a01bd716f6f72632b5c0f9a2c4dc2257d1e342138c13");

crypto.subtle.importKey("jwk", jwkKey, rsaImportParams, extractable, ["verify"]).then(function(key) {
    return crypto.subtle.verify("RSASSA-PKCS1-v1_5", key, signature, text);
}).then(function(result) {
    verified = result;

    shouldBeTrue("verified");

    finishJSTest();
});
