function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`FAIL: ${msg}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

function shouldBeArray(actual, expected, msg) {
    if (actual === null || expected === null) {
        if (actual !== expected)
            throw new Error(`FAIL: ${msg}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
        return;
    }
    if (actual.length !== expected.length)
        throw new Error(`FAIL: ${msg}: length mismatch: expected ${expected.length}, got ${actual.length}`);
    for (let i = 0; i < expected.length; i++) {
        if (actual[i] !== expected[i])
            throw new Error(`FAIL: ${msg}: index ${i}: expected ${JSON.stringify(expected[i])}, got ${JSON.stringify(actual[i])}`);
    }
}

let emoji = "\u{1F600}";
let emoji2 = "\u{1F601}";
let emoji3 = "\u{1F602}";

function makeSubstring(codeUnits, sliceLength) {
    let parent = String.fromCharCode(...codeUnits);
    return parent.slice(0, sliceLength);
}

let slice1A = makeSubstring([0xD83D, 0xDE00, 0x41], 2);
let slice1X = makeSubstring([0xD83D, 0xDE00, 0x58], 2);
let slice2Q = makeSubstring([0xD83D, 0xDE00, 0xD83D, 0xDE01, 0x51], 4);
let slice3Z = makeSubstring([0xD83D, 0xDE00, 0xD83D, 0xDE01, 0xD83D, 0xDE02, 0x5A], 6);

let full1A = emoji + "A";
let full2Q = emoji + emoji2 + "Q";
let full3Z = emoji + emoji2 + emoji3 + "Z";

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(/[\u{1F600}-\u{1F64F}]*A/u.exec(slice1A), null, "Greedy* range 1x slice");
    shouldBe(/[\u{1F600}-\u{1F64F}]*Z/u.exec(slice3Z), null, "Greedy* range 3x slice");
    shouldBe(/[\u{1F600}]*X/u.exec(slice1X), null, "Greedy* single-char-class slice");
    shouldBe(/[\u{1F600}-\u{1F64F}]+A/u.exec(slice1A), null, "Greedy+ range slice");
    shouldBe(/[\u{1F600}-\u{1F64F}]{1,3}A/u.exec(slice1A), null, "Greedy{1,3} range slice");
    shouldBe(/[\u{1F600}-\u{1F64F}]{0,5}Z/u.exec(slice3Z), null, "Greedy{0,5} range 3x slice");

    shouldBe(/[\u{1F600}-\u{1F64F}]*?Q/u.exec(slice2Q), null, "NonGreedy*? range 2x slice");
    shouldBe(/[\u{1F600}-\u{1F64F}]+?Q/u.exec(slice2Q), null, "NonGreedy+? range 2x slice");
    shouldBe(/[\u{1F600}]*?X/u.exec(slice1X), null, "NonGreedy*? single-char-class slice");

    shouldBe(/[\u{1F600}-\u{1F64F}]*A/v.exec(slice1A), null, "Greedy* range slice /v flag");
    shouldBe(/[\u{1F600}-\u{1F64F}]*?Q/v.exec(slice2Q), null, "NonGreedy*? range slice /v flag");

    shouldBe(/[a\u{1F600}]*A/u.exec(slice1A), null, "mixed-width class Greedy (slow path)");
    shouldBe(/[a\u{1F600}]*?Q/u.exec(slice2Q), null, "mixed-width class NonGreedy (slow path)");
    shouldBe(/[^a-z]*A/u.exec(slice1A), null, "inverted class Greedy (slow path)");

    shouldBeArray(/[\u{1F600}-\u{1F64F}]*A/u.exec(full1A), [emoji + "A"], "Greedy* full input match");
    shouldBeArray(/[\u{1F600}-\u{1F64F}]*Z/u.exec(full3Z), [emoji + emoji2 + emoji3 + "Z"], "Greedy* 3x full input match");
    shouldBeArray(/[\u{1F600}-\u{1F64F}]*/u.exec(emoji + emoji2), [emoji + emoji2], "Greedy* 2x no trailing");
    shouldBeArray(/[\u{1F600}-\u{1F64F}]*/u.exec(emoji + emoji2 + emoji3), [emoji + emoji2 + emoji3], "Greedy* 3x no trailing");
    shouldBeArray(/[\u{1F600}-\u{1F64F}]+A/u.exec(full1A), [emoji + "A"], "Greedy+ full input match");
    shouldBeArray(/[\u{1F600}-\u{1F64F}]{1,3}Z/u.exec(full3Z), [emoji + emoji2 + emoji3 + "Z"], "Greedy{1,3} full input match");
    shouldBeArray(/[\u{1F600}-\u{1F64F}]*?Q/u.exec(full2Q), [emoji + emoji2 + "Q"], "NonGreedy*? full input match");
    shouldBeArray(/[\u{1F600}-\u{1F64F}]*?$/u.exec(emoji + emoji2), [emoji + emoji2], "NonGreedy*? anchor 2x");
    shouldBeArray(/[\u{1F600}-\u{1F64F}]+?$/u.exec(emoji + emoji2 + emoji3), [emoji + emoji2 + emoji3], "NonGreedy+? anchor 3x");

    shouldBeArray(/[\u{1F600}-\u{1F64F}]*/u.exec(emoji + "B"), [emoji], "Greedy* stop at BMP");
    shouldBeArray(/[\u{1F600}-\u{1F64F}]*B/u.exec(emoji + "B"), [emoji + "B"], "Greedy* + trailing BMP");
    shouldBe(/[\u{1F600}-\u{1F64F}]*C/u.exec(emoji + "B"), null, "Greedy* + trailing BMP mismatch");

    shouldBeArray(/[\u{1F600}]*$/u.exec(emoji), [emoji], "Greedy* end anchor 1x");
    shouldBeArray(/[\u{1F600}]*$/u.exec(emoji + emoji), [emoji + emoji], "Greedy* end anchor 2x");

}
