function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL " + msg + ": got " + JSON.stringify(actual) + ", expected " + JSON.stringify(expected));
}

function roundTrip(values, msg) {
    let r = JSON.parse(JSON.stringify(values));
    shouldBe(r.length, values.length, msg + " length");
    for (let i = 0; i < values.length; ++i)
        shouldBe(r[i], values[i], msg + " " + i);
}

// Same length, same first and last 8 characters, different middle.
{
    let a = "https://example.com/aaaaaaaa/resources/file.js";
    let b = "https://example.com/bbbbbbbb/resources/file.js";
    roundTrip([a, b, a, b, a, a, b, b], "middle differs");
}

// Same prefix and suffix, different lengths.
{
    let values = [];
    for (let len = 17; len <= 64; ++len)
        values.push("prefix__" + "m".repeat(len - 16) + "__suffix");
    roundTrip(values.concat(values).concat(values.slice().reverse()), "lengths");
}

// Boundaries around the cached length range.
{
    let values = [];
    for (let len of [15, 16, 17, 18, 255, 256, 257, 1000]) {
        let s = "";
        for (let i = 0; i < len; ++i)
            s += String.fromCharCode(97 + (i * 7) % 26);
        values.push(s, s);
    }
    roundTrip(values, "boundaries");
}

// 16-bit sources, both representable and not representable in Latin-1.
{
    let wide = "あいう long string value with wide characters え";
    let latin1 = "a long latin1 string value éè";
    let r = JSON.parse('["あ", "' + latin1 + '", "' + wide + '", "' + latin1 + '", "' + wide + '"]');
    shouldBe(r[1], latin1, "16-bit source latin1 value");
    shouldBe(r[2], wide, "16-bit source wide value");
    shouldBe(r[3], latin1, "16-bit source latin1 repeat");
    shouldBe(r[4], wide, "16-bit source wide repeat");
    roundTrip([latin1, wide, latin1, wide], "8-bit and 16-bit mix");
}

// Escaped strings produce the same value as their unescaped spelling.
{
    let plain = "a string with a quote \" and a backslash \\ inside";
    let r = JSON.parse(JSON.stringify([plain, plain]) + "");
    shouldBe(r[0], plain, "escaped 0");
    shouldBe(r[1], plain, "escaped 1");
    let r2 = JSON.parse('["\\u0061 long string value abcdefgh", "a long string value abcdefgh"]');
    shouldBe(r2[0], "a long string value abcdefgh", "unicode escape");
    shouldBe(r2[1], "a long string value abcdefgh", "unicode escape repeat");
}

// Many distinct values thrashing the table, with object keys in between.
{
    let values = [];
    for (let i = 0; i < 5000; ++i)
        values.push({ url: "https://example.com/resources/" + (i % 700) + "/file.js", id: i });
    let r = JSON.parse(JSON.stringify(values));
    for (let i = 0; i < values.length; ++i) {
        shouldBe(r[i].url, values[i].url, "thrash url " + i);
        shouldBe(r[i].id, i, "thrash id " + i);
    }
}

// Cached values must stay valid across collections.
{
    let s = "a value that is long enough to be cached";
    let r1 = JSON.parse(JSON.stringify([s, s]));
    fullGC();
    let r2 = JSON.parse(JSON.stringify([s, s]));
    edenGC();
    let r3 = JSON.parse(JSON.stringify([s, s]));
    shouldBe(r1[0], s, "pre-GC");
    shouldBe(r2[1], s, "post-fullGC");
    shouldBe(r3[1], s, "post-edenGC");
}

// Reviver and eval paths.
{
    let s = "https://example.com/resources/benchmark-runner.js";
    let r = JSON.parse(JSON.stringify([s, { k: s }, s]), (k, v) => v);
    shouldBe(r[2], s, "reviver");
    shouldBe(r[1].k, s, "reviver nested");
    let e = eval("(" + JSON.stringify([s, { k: s }, s]) + ")");
    shouldBe(e[2], s, "eval");
}
