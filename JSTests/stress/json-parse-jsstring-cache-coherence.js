function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL " + msg + ": got " + JSON.stringify(actual) + ", expected " + JSON.stringify(expected));
}

// JSONAtomStringCache hashes by (firstChar, lastChar, length), so strings that
// differ only in the middle collide on the same slot. makeIdentifier (keys) and
// makeJSString (values) share m_cache; makeIdentifier must invalidate the
// parallel m_jsStrings entry on eviction so makeJSString never returns a stale
// JSString cell.

// 1. value -> colliding key -> value (the original repro)
{
    let r = JSON.parse('["axb",{"ayb":1},"ayb"]');
    shouldBe(r[2], "ayb", "value/key/value collision (axb/ayb)");
    let r2 = JSON.parse('["foo_x_bar",{"foo_y_bar":1},"foo_y_bar"]');
    shouldBe(r2[2], "foo_y_bar", "value/key/value collision (foo_?_bar)");
}

// 2. key -> value -> key -> value (reverse order)
{
    let r = JSON.parse('{"aXb":["aYb",{"aZb":2},"aZb","aYb"]}');
    shouldBe(r.aXb[2], "aZb", "key/value/key/value (aZb)");
    shouldBe(r.aXb[3], "aYb", "key/value/key/value (aYb)");
}

// 3. value <-> value collision without an intervening key
{
    let r = JSON.parse('["aaaa","abba","aaaa","abba"]');
    shouldBe(r[0], "aaaa", "value/value thrash 0");
    shouldBe(r[1], "abba", "value/value thrash 1");
    shouldBe(r[2], "aaaa", "value/value thrash 2");
    shouldBe(r[3], "abba", "value/value thrash 3");
}

// 4. 50x alternating thrash on one slot (key and value interleaved)
{
    let parts = [];
    for (let i = 0; i < 50; ++i)
        parts.push(i & 1 ? '{"aQb":0}' : '"aPb"');
    let r = JSON.parse('[' + parts.join(',') + ']');
    for (let i = 0; i < 50; ++i) {
        if (i & 1)
            shouldBe(r[i].aQb, 0, "thrash key " + i);
        else
            shouldBe(r[i], "aPb", "thrash value " + i);
    }
}

// 5. length boundaries (cache active for len 2-10 only)
{
    for (let len of [0, 1, 2, 9, 10, 11, 27, 28]) {
        let s = "v".repeat(len);
        let r = JSON.parse('["' + s + '","' + s + '"]');
        shouldBe(r[0], s, "len=" + len + " content");
        shouldBe(r[0].length, len, "len=" + len + " length");
        shouldBe(r[1], s, "len=" + len + " repeat");
    }
}

// 6. 16-bit value with middle-differs collision
{
    let r = JSON.parse('["a\u3042b",{"a\u3044b":1},"a\u3044b"]');
    shouldBe(r[2], "a\u3044b", "16-bit collision");
    shouldBe(r[0], "a\u3042b", "16-bit original");
}

// 7. value identical to a key in the same object
{
    let r = JSON.parse('{"hello":"hello","world":"hello"}');
    shouldBe(r.hello, "hello", "key==value same");
    shouldBe(r.world, "hello", "key==value cross");
}

// 8. across GC (cache is cleared on every collection)
{
    let r1 = JSON.parse('["click","focus","click"]');
    fullGC();
    let r2 = JSON.parse('["click","focus","click"]');
    shouldBe(r1[0], "click", "pre-GC");
    shouldBe(r2[0], "click", "post-GC content");
    shouldBe(r2[2], "click", "post-GC repeat");
    edenGC();
    let r3 = JSON.parse('["axb",{"ayb":1},"ayb"]');
    shouldBe(r3[2], "ayb", "post-edenGC collision");
}

// 9. eval('(...)') SloppyJSON path goes through the same makeJSString
{
    let r = eval('(["axb",{"ayb":1},"ayb"])');
    shouldBe(r[2], "ayb", "eval SloppyJSON collision");
    let r2 = eval('({"k":["idle","hover","idle"]})');
    shouldBe(r2.k[2], "idle", "eval repeat value");
}

// 10. reviver path (non-recursive parse) also uses makeJSString
{
    let r = JSON.parse('["axb",{"ayb":1},"ayb"]', (k, v) => v);
    shouldBe(r[2], "ayb", "reviver collision");
}

// 11. many distinct (first,last,len) keys evicting cached values
{
    let values = [];
    let keys = [];
    for (let i = 0; i < 20; ++i) {
        values.push("v" + String.fromCharCode(65 + i) + "x");
        keys.push("v" + String.fromCharCode(97 + i) + "x");
    }
    let json = '[';
    for (let i = 0; i < 20; ++i)
        json += '"' + values[i] + '",{"' + keys[i] + '":0},"' + keys[i] + '",';
    json += '0]';
    let r = JSON.parse(json);
    for (let i = 0; i < 20; ++i) {
        shouldBe(r[i * 3], values[i], "many-evict value " + i);
        shouldBe(r[i * 3 + 2], keys[i], "many-evict key-as-value " + i);
    }
}
