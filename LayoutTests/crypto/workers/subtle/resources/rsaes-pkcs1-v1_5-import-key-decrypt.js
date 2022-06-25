importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test decrypting using RSAES-PKCS1-v1_5 with an imported key in workers");

jsTestIsAsync = true;

var extractable = false;
var cipherText = hexStringToUint8Array("87ca360442614b6590ea51ba84eb081ecda83c5a19e008ed21a6b93c78a7980348f9eac9a8120a6b67326b58ff6e7a10bec113b01f6bd18a04b82d159eb3532c03d4fca597450333cd4df81456b7b864a276cc3a7551964ccc16de86e2bc7a42981f29a0855fc151692d802f6d0b6fdcc0cc01977c579acde220b60d418a480f0ccdfd370f7e045114be39f03dfd516b51bcba6af20663cb2404e4e8c5b4c239936c672c3c2de4f4f3382cdc68f277fc66ed06a0642584ead53cd676673e8cecf59fe23cbe0a59cd82c8d56187eff6cda9b9e600b9890f42c65ee9a4f16f87c1c0b53cecb6fb59a4de165cc828d6bc5b5bf152e6c3060b54fd4e11b037668e4d");
var jwkKey = {
    kty: "RSA",
    alg: "RSA1_5",
    use: "enc",
    key_ops: ["decrypt", "unwrapKey"],
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
var expectedPlainText = "Hello, World!";

crypto.subtle.importKey("jwk", jwkKey, "RSAES-PKCS1-v1_5", extractable, ["decrypt"]).then(function(key) {
    return crypto.subtle.decrypt("RSAES-PKCS1-v1_5", key, cipherText);
}).then(function(result) {
    plainText = result;

    shouldBe("bytesToASCIIString(plainText)", "expectedPlainText");

    finishJSTest();
});
