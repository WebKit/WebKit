// META: script=./resources/common.js

test(() => {
    const max = 10000;
    const wrappedKey = hexStringToUint8Array( "b50b299894672c25341db938ad7dc3f87f05e29a36e062b08f7022f9bb7ee41a6d503bfa460e63333c2d1b6fe5b62169276f511ed928cdf0ad9af40807b8a5674af07016e5af5476f6aec5266321e300eb");
    const rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
    const aesGcmParams = {
        name: "aes-gcm",
        iv: asciiToUint8Array("jnOw99oOZFLIEPMr"),
    };
    crypto.subtle.importKey('raw', rawKey, "aes-gcm", false, ["unwrapKey"]).then(key => {
        for (let i = 0; i < max; i++)
            crypto.subtle.unwrapKey("raw", wrappedKey, key, aesGcmParams, { name: "ECDH", namedCurve: "P-256" }, false, [ ]);
    });
}, "Test passed if no crashes.");
