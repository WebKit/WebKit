importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test wrapping a JWK RSA public key with AES-CBC using an imported key in workers");

jsTestIsAsync = true;

var extractable = true;
var jwkKey = {
    kty: "RSA",
    alg: "RSA-OAEP",
    use: "enc",
    key_ops: ["encrypt"],
    ext: true,
    n: "rcCUCv7Oc1HVam1DIhCzqknThWawOp8QLk8Ziy2p10ByjQFCajoFiyuAWl-R1WXZaf4xitLRracT9agpzIzc-MbLSHIGgWQGO21lGiImy5ftZ-D8bHAqRz2y15pzD4c4CEou7XSSLDoRnR0QG5MsDhD6s2gV9mwHkrtkCxtMWdBi-77as8wGmlNRldcOSgZDLK8UnCSgA1OguZ989bFyc8tOOEIb0xUSfPSz3LPSCnyYz68aDjmKVeNH-ig857OScyWbGyEy3Biw64qun3juUlNWsJ3zngkOdteYWytx5Qr4XKNs6R-Myyq72KUp02mJDZiiyiglxML_i3-_CeecCw",
    e: "AQAB"
};
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var aesCbcParams = {
    name: "aes-cbc",
    iv: asciiToUint8Array("jnOw99oOZFLIEPMr"),
}
var expectWrappedKey = "995a93379575a0c677a7d48dbe7d496fe4d8c9be49cdaf48e88398a769c2825d94396edb75683ed54e3ca0ac1f352f6b92486ef68186ef37d2b6be27ddd4bdaafbea98ec8c0dfc62353714f11219d4c69271a06507909f6dc50fd136f06265e5bfe9b2de09a704db4b4acd1d2834eefaab814484b70d77984455a1bad47e94cedc467939ea7495387087aff0eec9dfa4bf6c5baa6df37328a7068128930adc129a1874c323f315e993a27c667f1e02ffab5cc235bb7d270696875f43bb198f453cdef134ffe171c855c4641678527819dce552e4cd97fb30404075869aba085634327774cd7b71e7d0ea306f2d92d64658778897489fff205645e1a1b57832349ef1e6abef7c6780aa085b7d55c55693d69cf439497925dc8239791130d4387387188f2dea13d2825b2690456df80f6f1694351c8f33eb37bc44f2ad3f8f6d0a0e98c1510c6a5f64209288516aec0b30f85ed0b129a5ffc4840a9b5f0374c37304ad4f3d81572417813172929dd08f8a853ab57d3bc5d72eeeca932d8a05d3eb74d2ceba39b87cd7113abb30de3d05448162b50b17de8603fa4b5e7f8ab267cc6b8fdc830d76a35eb4078597b6301176";

crypto.subtle.importKey("raw", rawKey, "aes-cbc", extractable, ["wrapKey"]).then(function(result) {
    wrappingKey = result;
    return crypto.subtle.importKey("jwk", jwkKey, { name: "rsa-oaep", hash: "sha-1" }, extractable, ["encrypt"]);
}).then(function(result) {
    key = result;
    return crypto.subtle.wrapKey("jwk", key, wrappingKey, aesCbcParams);
}).then(function(result) {
    wrappedKey = result;

    shouldBe("bytesToHexString(wrappedKey)", "expectWrappedKey");

    finishJSTest();
});
