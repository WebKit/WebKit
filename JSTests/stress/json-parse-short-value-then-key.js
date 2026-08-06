function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL " + msg + ": got " + JSON.stringify(actual) + ", expected " + JSON.stringify(expected));
}

// A short string first seen as a value is cached without being atomized. The
// same string later used as a key must still produce a working property, and a
// subsequent value occurrence must still return the right string.
for (let i = 0; i < 3; ++i) {
    let r = JSON.parse('["prop_a1", {"prop_a1": 1}, {"prop_a1": 2, "z": 3}, "prop_a1", {"prop_a1": 4}]');
    shouldBe(r[0], "prop_a1", "value before key");
    shouldBe(r[1].prop_a1, 1, "key after value");
    shouldBe(r[1][r[0]], 1, "computed lookup with parsed value");
    shouldBe(Object.keys(r[1])[0], "prop_a1", "Object.keys");
    shouldBe(r[2].prop_a1, 2, "key again (existing transition)");
    shouldBe(r[2].z, 3, "second key");
    shouldBe(r[3], "prop_a1", "value after key");
    shouldBe(r[4].prop_a1, 4, "key after value after key");

    let o = {};
    o[r[3]] = 5;
    shouldBe(o.prop_a1, 5, "parsed value used as a key elsewhere");
    shouldBe("prop_a1" in r[1], true, "in");

    fullGC();
}

// Value evicted by a colliding value, then used as key.
{
    let r = JSON.parse('["kQx", "kRx", {"kQx": 1, "kRx": 2}, "kQx", "kRx"]');
    shouldBe(r[2].kQx, 1, "evicted value as key 1");
    shouldBe(r[2].kRx, 2, "evicted value as key 2");
    shouldBe(r[3], "kQx", "value after eviction 1");
    shouldBe(r[4], "kRx", "value after eviction 2");
}

// 16-bit source with Latin-1 and non-Latin-1 short values and keys.
{
    let r = JSON.parse('["v\\u00e9", "v\\u3042", {"v\\u00e9": 1, "v\\u3042": 2}, "v\\u00e9", "v\\u3042", "いk", {"いk": 3}]');
    shouldBe(r[0], "vé", "latin1 value");
    shouldBe(r[1], "vあ", "utf16 value");
    shouldBe(r[2]["vé"], 1, "latin1 key");
    shouldBe(r[2]["vあ"], 2, "utf16 key");
    shouldBe(r[3], "vé", "latin1 value again");
    shouldBe(r[4], "vあ", "utf16 value again");
    shouldBe(r[5], "いk", "utf16 source value");
    shouldBe(r[6]["いk"], 3, "utf16 source key");

    let s = JSON.parse('["\\u3042", "\\u3042", {"\\u3042": 1}, "\\u3042"]');
    shouldBe(s[0], "あ", "single non-latin1 char value");
    shouldBe(s[1], "あ", "single non-latin1 char value again");
    shouldBe(s[2]["あ"], 1, "single non-latin1 char key");
    shouldBe(s[3], "あ", "single non-latin1 char value after key");
}

// Repeated short values across many parses share content correctly across GCs.
{
    let json = JSON.stringify(Array.from({ length: 200 }, (_, i) => "id-" + (i % 7)));
    for (let i = 0; i < 200; ++i) {
        let r = JSON.parse(json);
        for (let j = 0; j < r.length; j += 37)
            shouldBe(r[j], "id-" + (j % 7), "repeated parse " + i + "/" + j);
        if (i % 50 == 0)
            edenGC();
    }
}
