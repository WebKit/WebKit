// META: script=./resources/common.js

test(() => {
    const max = 10000;
    const plainText = asciiToUint8Array("Hello, World!");
    const jwkKey = {
        kty: "RSA",
        alg: "RSA-OAEP",
        use: "enc",
        key_ops: ["encrypt", "wrapKey"],
        ext: true,
        n: "rcCUCv7Oc1HVam1DIhCzqknThWawOp8QLk8Ziy2p10ByjQFCajoFiyuAWl-R1WXZaf4xitLRracT9agpzIzc-MbLSHIGgWQGO21lGiImy5ftZ-D8bHAqRz2y15pzD4c4CEou7XSSLDoRnR0QG5MsDhD6s2gV9mwHkrtkCxtMWdBi-77as8wGmlNRldcOSgZDLK8UnCSgA1OguZ989bFyc8tOOEIb0xUSfPSz3LPSCnyYz68aDjmKVeNH-ig857OScyWbGyEy3Biw64qun3juUlNWsJ3zngkOdteYWytx5Qr4XKNs6R-Myyq72KUp02mJDZiiyiglxML_i3-_CeecCw",
        e: "AQAB"
    };
    const rsaOaepParams = {
        name: "rsa-oaep"
    };
    crypto.subtle.importKey('jwk', jwkKey, {name: "RSA-OAEP", hash: "SHA-1"}, false, ["encrypt"]).then(key => {
        for (let i = 0; i < max; i++)
            crypto.subtle.encrypt(rsaOaepParams, key, plainText);
    });
}, "Test passed if no crashes.");
