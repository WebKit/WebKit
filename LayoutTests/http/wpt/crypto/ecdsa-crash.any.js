// META: script=./resources/common.js

test(() => {
    const max = 10000;
    const jwkPrivateKey = {
        kty: "EC",
        crv: "P-256",
        x: "1FSVWieTvikFkG1NOyhkUCaMbdQhxwH6aCu4Ez-sRtA",
        y: "9jmNTLqM4cjBhdAnHcNI9YQV3O8LFmo-EdZWk8ntAaI",
        d: "ppxBSov3N8_AUcisAuvmLV4yE8e_L_BLE8bZb9Z1Xjg",
    };
    const ecdsaParams = {
        name: "ECDSA",
        hash: "SHA-256",
    };
    const data = asciiToUint8Array("Hello, World!");

    crypto.subtle.importKey("jwk", jwkPrivateKey, { name: "ECDSA", namedCurve: "P-256" }, false, ["sign"]).then(key => {
        for (let i = 0; i < max; i++)
            crypto.subtle.sign(ecdsaParams, key, data);
    });
}, "Test passed if no crashes.");
