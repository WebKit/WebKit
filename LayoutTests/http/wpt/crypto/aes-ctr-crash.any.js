// META: script=./resources/common.js

test(() => {
    const max = 10000;
    const plainText = asciiToUint8Array("Hello, World!");
    const aesCtrParams = {
        name: "aes-ctr",
        counter: asciiToUint8Array("jnOw99oOZFLIEPMr"),
        length: 8,
    };
    const rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
    crypto.subtle.importKey('raw', rawKey, "aes-ctr", false, ["encrypt"]).then(key => {
        for (let i = 0; i < max; i++)
            crypto.subtle.encrypt(aesCtrParams, key, plainText);
    });
}, "Test passed if no crashes.");
