importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test unwrapping a JWK oct key with RSA-OAEP using an imported key in workers");

jsTestIsAsync = true;

var extractable = true;
var rawKey = asciiToUint8Array("jnOw99oOZFLIEPMr");
var jwkKey = {
    kty: "RSA",
    alg: "RSA-OAEP",
    use: "enc",
    key_ops: ["unwrapKey"],
    ext: true,
    n: "lxHN0N9VRZ0_pl0xv3-NXx70WnjkODSkQ5LjHXTFy3DOQsvkagFzD9HqYQezCmcewLjdK5PLwSesDoMdfL6tusBHcvyit1kvydYFQ3NLbENNkYsiBG5_nW4IQGL6JKbZ5iGdUop98QHKm6YZR1u4zrAtxM6bVEo05VvhjRS0M8yWoZVi-7Vdsc0LqI0Qdq_NoctX5Fu-AqiBN7Uo1HkYGcP2oC82J_J5cjw98BQiP5kDWThq9RK-X6S-EUALx_m4iG6iOYKTA3SQyf1xBqFaXXoEJjcckbOqkegecz5b-YWUh8iZPvhwnt-RZwpIbLJgKwz19ndkn9KvoEEw7YbEow",
    e: "AQAB",
    d: "cj5DkDakjM2bKduGWJREO-_zyEtuA1dD9doqKMd7IRuA0CDS7puEAS20-oXRDwfmyMXEdEUDrGGtCxh6fzDPvs_T-JA3GUK4EgHo3xZcrlXDXlKCeil6Fnr0gISZOIh5dkBrcdVL4quBJe4ZZc5mVuAC7Ld13et0TxMJ4iALGrPuqPVUOGSYIcZ9idx5zKKBWhY3tPggEdKpnHBmPfTRO4yZaf0Nw1QXrgSMZY9ejeuaurAh4Q8o4-6-r8O2LUe7ufMh_ccKkXISEh4KdOnT17EM9BQTn9UNS9GoK2ZZU0U3io5DSu_kpasr4uOVWcGlE2wczOv2nkGwG39F3sFF0Q",
    p: "x5vnco5j-TD6hTOzyN4DhkZ44m05NycxT6SUE2qTurT3-uze_L7TYutLRIRkovRMhTHZAr2pziRlasEs13PEz9Zvx1I_T68srsonrdbak-SFMecM7EjHc5C-J13gXhw9HIW28_Sx9rQ-JkGwEwE9PEdIUfuvdqpgh3SmXwPJrEs",
    q: "wb9vllg_2n-kNge0bThg_7xu1UwTzipM8vxSUkkV2IipJKIAekkU3aAB8LoPhUI0-17pSGw3ETOO27t163TI9qIPpzLbhTH9aUi7qLGbKlzPlgnqP43Z0LHxc3xKDgit-Ar29QLaX2uoJBX6VVWvhmh7BIPDHNVM5GZjwWORYgk",
    dp: "C2c8sa6wx2uk5Dcv7inAycr83PKgciYrCwG78-AC0IfGIu-lTYsZSG1ov2FQ3n5WYMWYQC_Vo5EwugiPJz_V3onBmQF53HOFefbSjXvYwNotQcyRUG5X9qIuOtGCH949H4QED6vK_u0NH-JgzLUlamwoFYbrXzwch6CCYKs2ukE",
    dq: "hbtRloDLclHwUqr2yvzDV0IFbozYjtF706x-VfXEcnXB6ls34TBYirFLJZIH7H9KeseEVkz7pY_k5555QlCV9kbebxYXl9RtiiJ-BW6yH4d4caPeYIfU9MweUQxVQWKUUkWfOHcDrCFvKZlR9Vzzjt7HKtKX9mr0bCKQcIf9baE",
    qi: "a-7hUTTnclUPKOfSgH8zEKGJ-AvdFEzxvZ5sq46Qf2MbORxVjN4dJamVvM-FoqcwN-9cuUlyr9bSFTwUBW4vXa8Xj9a8JfViuMCqzR-mL1rGIUQ5ARGhNcSsRlyKTqz5BlWlVKmXIx_p-DeVwPWiJJy4k_FqyBxrnxkzomHfrxk",
};
var wrappedJwkKey = hexStringToUint8Array("5bf8b98ac4f00c04fbe85900296e4b411acede31c57d1689d6a9678966bf4eb2019da921deb7a410cc0ef1ebecfbd2db6105168d133a5dbeb29f69efc61b4d63d37feb8460bbd319459b180f8661d01d087a929413cfa4fd94e355137fbb54413837e47c4e114e82337261a5bbfe20637b6b08dc4974ef1fbefcaac5a4f463021b810ddf7c9a2ddecb119e82602dc14135a3787ed8ab40aedca74755d3518b35dc38ebe85dd13b5500bae3bd5e2f25b6dfcf8c9265ddd0473bf4f69d3d40f2409462d35a7be7902029bd8f7c2ea1d879b07eda52638e93722cece9658e1acd6768d78d00b214349555853511762919854b4991fa110a08b6e6b9e6d93484f143");

crypto.subtle.importKey("jwk", jwkKey, {name: "rsa-oaep", hash: "sha-1"}, extractable, ["unwrapKey"]).then(function(unwrappingKey) {
    return crypto.subtle.unwrapKey("jwk", wrappedJwkKey, unwrappingKey, "rsa-oaep", "AES-CBC", extractable, ["encrypt", "decrypt"]);
}).then(function(cryptoKey) {
    return crypto.subtle.exportKey("jwk", cryptoKey);
}).then(function(result) {
    unwrappedKey = result;

    shouldBe("unwrappedKey.kty", "'oct'");
    shouldBe("Base64URL.parse(unwrappedKey.k)", "rawKey");
    shouldBe("unwrappedKey.alg", "'A128CBC'");
    shouldBe("unwrappedKey.key_ops", "['decrypt', 'encrypt']");
    shouldBe("unwrappedKey.ext", "true");

    finishJSTest();
});
