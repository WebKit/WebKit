// META: script=./resources/common.js

test(() => {
    const max = 10000;
    const rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
    const salt = asciiToUint8Array("jnOw99oO");
    const info = asciiToUint8Array("jnOw99oO");
    crypto.subtle.importKey("raw", rawKey, "HKDF", false, ["deriveKey"]).then(key => {
        for (let i = 0; i < max; i++)
            crypto.subtle.deriveKey({name: "HKDF", salt: salt, info: info, hash: "sha-1"}, key, {name: "hmac", hash: "sha-1"}, false, ['sign', 'verify']);
    });
}, "Test passed if no crashes.");
