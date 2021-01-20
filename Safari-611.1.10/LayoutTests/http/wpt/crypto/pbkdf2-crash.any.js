// META: script=./resources/common.js

test(() => {
    const max = 10000;
    crypto.subtle.importKey('raw', new Uint8Array(32), {name: 'PBKDF2', hash: {name: 'SHA-256'}}, false, ['deriveBits']).then(key => {
        for (let i = 0; i < max; i++)
            crypto.subtle.deriveBits({name: 'PBKDF2', hash: {name: 'SHA-256'}, salt: new Uint8Array(32), iterations: 1000}, key, 256);
    });
}, "Test passed if no crashes.");
