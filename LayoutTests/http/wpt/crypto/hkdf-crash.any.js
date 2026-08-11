// META: script=./resources/common.js

test(() => {
    const max = 10000;
    const rawKey = hexStringToUint8Array("0b0b0b0b0b0b0b0b0b0b0b");
    const info = hexStringToUint8Array("f0f1f2f3f4f5f6f7f8f9");
    const salt = hexStringToUint8Array("000102030405060708090a0b0c");
    crypto.subtle.importKey("raw", rawKey, "HKDF", false, ["deriveBits"]).then(key => {
        for (let i = 0; i < max; i++)
            crypto.subtle.deriveBits({name: "HKDF", salt: salt, info: info, hash: "sha-1"}, key, 336);
    });
}, "Test passed if no crashes.");
