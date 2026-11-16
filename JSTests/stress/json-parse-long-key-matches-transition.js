function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL " + msg + ": got " + JSON.stringify(actual) + ", expected " + JSON.stringify(expected));
}

// Keys of 16 characters or more that match, or nearly match, the previous object's transition.
{
    let key = "expressionLocation";
    let objects = [];
    for (let i = 0; i < 20; ++i)
        objects.push({ [key]: i, other: i });
    let r = JSON.parse(JSON.stringify(objects));
    for (let i = 0; i < 20; ++i) {
        shouldBe(r[i][key], i, "long key " + i);
        shouldBe(Object.keys(r[i]).join(), key + ",other", "long key order " + i);
    }

    let mixed = JSON.parse('[{"expressionLocation":1},{"expressionLocatioN":2},{"expressionLocationX":3},{"expressionLocatio":4},{"expressionLocatio\\u006e":5},{"expressionLocation":6}]');
    shouldBe(mixed[0].expressionLocation, 1, "exact");
    shouldBe(mixed[1].expressionLocatioN, 2, "last character differs");
    shouldBe(mixed[1].expressionLocation, undefined, "last character differs, no match");
    shouldBe(mixed[2].expressionLocationX, 3, "longer");
    shouldBe(mixed[3].expressionLocatio, 4, "shorter");
    shouldBe(mixed[4].expressionLocation, 5, "escaped spelling");
    shouldBe(mixed[5].expressionLocation, 6, "exact again");

    let longest = "k".repeat(100);
    let r2 = JSON.parse(JSON.stringify([{ [longest]: 1 }, { [longest]: 2 }, { [longest + "k"]: 3 }]));
    shouldBe(r2[1][longest], 2, "100-character key");
    shouldBe(r2[2][longest + "k"], 3, "101-character key");

    let tail = '[{"' + key + '":1},{"' + key + '"';
    let threw = false;
    try {
        JSON.parse(tail);
    } catch (e) {
        threw = e instanceof SyntaxError;
    }
    shouldBe(threw, true, "long key at end of input");
}

// The same keys in a 16-bit source, which compares every key length through the scanning path.
{
    let json = '[{"expressionLocation":1,"a":"あ"},{"expressionLocation":2,"a":"あ"},{"expressionLocatioN":3},{"expressionLocatio\\u006e":4},{"kあ":5},{"kあ":6},{"x":7},{"x":8}]';
    let r = JSON.parse(json);
    shouldBe(r[0].expressionLocation, 1, "16-bit exact 0");
    shouldBe(r[1].expressionLocation, 2, "16-bit exact 1");
    shouldBe(r[1].a, "あ", "16-bit value");
    shouldBe(r[2].expressionLocatioN, 3, "16-bit last character differs");
    shouldBe(r[3].expressionLocation, 4, "16-bit escaped spelling");
    shouldBe(r[5]["kあ"], 6, "16-bit wide key");
    shouldBe(r[7].x, 8, "16-bit short key");
}
