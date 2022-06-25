// META: script=./resources/common.js

test(() => {
    const max = 10000;
    const plainText = asciiToUint8Array("Hello, World!");
    const aesGcmParams = {
        name: "aes-gcm",
        iv: asciiToUint8Array("jnOw99oOZFLIEPMr"),
    };
    const rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
    crypto.subtle.importKey('raw', rawKey, "aes-gcm", false, ["encrypt"]).then(key => {
        for (let i = 0; i < max; i++)
            crypto.subtle.encrypt(aesGcmParams, key, plainText);
    });
}, "Test passed if no crashes.");
