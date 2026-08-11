function shouldBe(actual, expected, name) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(name + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

{
    let parent = "ZabcX";
    let input = parent.slice(0, 4);
    shouldBe(/abcX|wxyz/.exec(input), null, "scalar-failed must not enter body (4-byte pattern)");
}

{
    let parent = "Z".repeat(4000) + "abcdSeCrEtDaTa";
    let input = parent.slice(0, 4004);
    shouldBe(/abcdSeCrEtDaTa|wxyzABCDEF1234/.exec(input), null, "scalar-matched must not OOB (14-byte pattern, slice)");
}

{
    let parent = "Z".repeat(17) + "abcdEFGH";
    let input = parent.slice(0, 21);
    shouldBe(/abcdEFGH|wxyz1234/.exec(input), null, "scalar-matched must not OOB (8-byte pattern, scalar tail)");
}

{
    let parent = "Z".repeat(100) + "abcdEFGHIJ";
    let input = parent.slice(0, 104);
    shouldBe(/abcdEFGHIJ|wxyz/.exec(input), null, "scalar-matched must not OOB (asymmetric alt lengths)");
}

shouldBe(/abcd|wxyz/.exec("ZZZZabcd")[0], "abcd", "in-bounds alt1 at end");
shouldBe(/abcd|wxyz/.exec("ZZZZwxyz")[0], "wxyz", "in-bounds alt2 at end");
shouldBe(/abcd|wxyz/.exec("abcd")[0], "abcd", "length == patternLength");
shouldBe(/abcd|wxyz/.exec("Z".repeat(17) + "abcd")[0], "abcd", "scalar tail range, in-bounds");
shouldBe(/abcdefgh|wxyz1234/.exec("Z".repeat(4000) + "abcdefgh")[0], "abcdefgh", "SIMD loop range, long pattern, in-bounds");
shouldBe(/abcd|wxyz/i.exec("ZZZZABCD")[0], "ABCD", "ignoreCase (masked path)");
shouldBe("abcdZwxyzZabcd".match(/abcd|wxyz/g), ["abcd", "wxyz", "abcd"], "global, multiple matches");
