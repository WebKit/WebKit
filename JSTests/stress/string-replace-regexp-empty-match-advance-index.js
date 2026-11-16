function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

let s = "\uD800\uDC00";

class SlowRegExp extends RegExp {}

{
    let fast_g  = s.replace(/(?:)/g,  "X");
    let fast_gu = s.replace(/(?:)/gu, "X");
    let slow_g  = s.replace(new SlowRegExp("(?:)", "g"),  "X");
    let slow_gu = s.replace(new SlowRegExp("(?:)", "gu"), "X");

    shouldBe(slow_g,  "X\uD800X\uDC00X");
    shouldBe(slow_gu, "X\uD800\uDC00X");

    shouldBe(fast_g,  slow_g);
    shouldBe(fast_gu, slow_gu);

    shouldBe(fast_g.length,  5);
    shouldBe(fast_gu.length, 4);
}

{
    let fast_g  = s.replace(/(?:)/g,  "[$&]");
    let fast_gu = s.replace(/(?:)/gu, "[$&]");
    let slow_g  = s.replace(new SlowRegExp("(?:)", "g"),  "[$&]");
    let slow_gu = s.replace(new SlowRegExp("(?:)", "gu"), "[$&]");

    shouldBe(fast_g,  slow_g);
    shouldBe(fast_gu, slow_gu);
    shouldBe(fast_g,  "[]\uD800[]\uDC00[]");
    shouldBe(fast_gu, "[]\uD800\uDC00[]");
}

{
    function collectOffsets(re) {
        let offsets = [];
        s.replace(re, function(match, offset) { offsets.push(offset); return ""; });
        return offsets.join(",");
    }

    shouldBe(collectOffsets(/(?:)/g),  "0,1,2");
    shouldBe(collectOffsets(/(?:)/gu), "0,2");
    shouldBe(collectOffsets(new SlowRegExp("(?:)", "g")),  "0,1,2");
    shouldBe(collectOffsets(new SlowRegExp("(?:)", "gu")), "0,2");
}

{
    shouldBe(s.replaceAll(/(?:)/g,  "X"), "X\uD800X\uDC00X");
    shouldBe(s.replaceAll(/(?:)/gu, "X"), "X\uD800\uDC00X");
}

{
    let fast_g  = s.replace(/a*/g,  "_");
    let fast_gu = s.replace(/a*/gu, "_");
    let slow_g  = s.replace(new SlowRegExp("a*", "g"),  "_");
    let slow_gu = s.replace(new SlowRegExp("a*", "gu"), "_");

    shouldBe(fast_g,  slow_g);
    shouldBe(fast_gu, slow_gu);
    shouldBe(fast_g,  "_\uD800_\uDC00_");
    shouldBe(fast_gu, "_\uD800\uDC00_");
}

{
    let fast_gv = s.replace(/(?:)/gv, "X");
    let slow_gv = s.replace(new SlowRegExp("(?:)", "gv"), "X");
    shouldBe(fast_gv, slow_gv);
    shouldBe(fast_gv, "X\uD800\uDC00X");
}

{
    let s2 = "\uD800\uDC00\uD801\uDC01";
    let fast_g  = s2.replace(/(?:)/g,  "|");
    let fast_gu = s2.replace(/(?:)/gu, "|");

    shouldBe(fast_g,  "|\uD800|\uDC00|\uD801|\uDC01|");
    shouldBe(fast_gu, "|\uD800\uDC00|\uD801\uDC01|");
    shouldBe(fast_g,  s2.replace(new SlowRegExp("(?:)", "g"),  "|"));
    shouldBe(fast_gu, s2.replace(new SlowRegExp("(?:)", "gu"), "|"));
}

{
    let s3 = "\uD800x";
    shouldBe(s3.replace(/(?:)/g,  "_"), "_\uD800_x_");
    shouldBe(s3.replace(/(?:)/gu, "_"), "_\uD800_x_");
}
