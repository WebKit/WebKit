// META: title=WebCryptoAPI: unwrapKey() with invalid AES-KW wrapped keys

// Test vector from RFC 3394, section 4.1: wrap 128 bits of key data with a
// 128-bit KEK.
var kekBytes = new Uint8Array([0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f]);
var keyBytes = new Uint8Array([0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff]);
var wrappedKeyBytes = new Uint8Array([0x1f, 0xa6, 0x8b, 0x0a, 0x81, 0x12, 0xb4, 0x47,
    0xae, 0xf3, 0x4b, 0xd8, 0xfb, 0x5a, 0x7b, 0x82, 0x9d, 0x3e, 0x86, 0x23,
    0x71, 0xd2, 0xcf, 0xe5]);

function importKek() {
    return crypto.subtle.importKey("raw", kekBytes, "AES-KW", false, ["unwrapKey"]);
}

function unwrapKey(kek, wrappedKey) {
    return crypto.subtle.unwrapKey("raw", wrappedKey, kek, "AES-KW", "AES-CBC", true, ["encrypt", "decrypt"]);
}

promise_test(async () => {
    const kek = await importKek();
    const key = await unwrapKey(kek, wrappedKeyBytes);
    const exported = new Uint8Array(await crypto.subtle.exportKey("raw", key));
    assert_array_equals(exported, keyBytes);
}, "AES-KW unwrapKey with a valid wrapped key");

promise_test(async (t) => {
    const kek = await importKek();
    const tamperedKeyBytes = wrappedKeyBytes.slice();
    tamperedKeyBytes[tamperedKeyBytes.length - 1] ^= 1;
    return promise_rejects_dom(t, "OperationError", unwrapKey(kek, tamperedKeyBytes));
}, "AES-KW unwrapKey with a tampered wrapped key");

for (const keyLength of [128, 192, 256]) {
    for (const wrappedKeyLength of [0, 1, 2, 3, 4, 5, 6, 7, 8, 16, 20, 23, 25, 31, 33]) {
        promise_test(async (t) => {
            const kek = await crypto.subtle.importKey("raw", new Uint8Array(keyLength / 8), "AES-KW", false, ["unwrapKey"]);
            return promise_rejects_dom(t, "OperationError", unwrapKey(kek, new Uint8Array(wrappedKeyLength)));
        }, `AES-KW with a ${keyLength}-bit key rejects wrapped key of length ${wrappedKeyLength} with OperationError`);
    }
}
