function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`bad error: ${error}`);
}

function rope(a, b) {
    return a + b;
}
noInline(rope);

function substring(string, start, end) {
    return string.slice(start, end);
}
noInline(substring);

let base = "0123456789abcdefghijklmnopqrstuvwxyz";
let count = 0;
let object = {
    toString() {
        ++count;
        return "xy";
    }
};
let thrower = {
    toString() {
        throw new RangeError("thrower");
    }
};

function test(i) {
    shouldBe("7".padStart(3, rope("0", "1")), "017");
    shouldBe("7".padEnd(6, rope("0", "1")), "701010");
    shouldBe("7".padStart(4, substring(base, 10, 12)), "aba7");
    shouldBe("7".padEnd(4, substring(base, 10, 12)), "7aba");
    shouldBe("7".padStart(3, rope("\u3042", "\u3044")), "\u3042\u30447");
    shouldBe("\u3042".padEnd(3, rope("a", "b")), "\u3042ab");
    shouldBe("7".padStart(3, 0), "007");
    shouldBe("7".padStart(5, 12 + (i & 1)), (i & 1) ? "13137" : "12127");
    shouldBe("7".padEnd(5, 1.5), "71.51");
    shouldBe("7".padStart(5, true), "true7");
    shouldBe("7".padStart(5, null), "null7");
    shouldBe("7".padStart(3, undefined), "  7");
    shouldBe("7".padEnd(3), "7  ");
    shouldBe("7".padEnd(3, rope("", "")), "7");

    let before = count;
    shouldBe("7".padStart(4, object), "xyx7");
    shouldBe(count, before + 1);
    shouldBe("abc".padStart(2, object), "abc");
    shouldBe(count, before + 1);
}
noInline(test);

function testLong() {
    let long = "7".padStart(1030, rope("ab", "c"));
    shouldBe(long.length, 1030);
    shouldBe(long.slice(0, 5), "abcab");
    shouldBe(long.slice(1024), "bcabc7");

    long = "7".padEnd(1100, rope("0123456789", "abc"));
    shouldBe(long.length, 1100);
    shouldBe(long.slice(0, 15), "70123456789abc0");
    shouldBe(long.slice(1090), "abc0123456");

    long = "7".padStart(1030, substring(base, 10, 23));
    shouldBe(long.length, 1030);
    shouldBe(long.slice(0, 14), "abcdefghijklma");
    shouldBe(long.slice(1025), "lmab7");

    long = "7".padStart(1030, 0);
    shouldBe(long.length, 1030);
    shouldBe(long.slice(1026), "0007");

    long = "7".padEnd(1030);
    shouldBe(long.length, 1030);
    shouldBe(long.slice(0, 3), "7  ");

    let before = count;
    long = "7".padEnd(1030, object);
    shouldBe(long.length, 1030);
    shouldBe(long.slice(0, 4), "7xyx");
    shouldBe(count, before + 1);
}
noInline(testLong);

for (let i = 0; i < testLoopCount; ++i)
    test(i);

for (let i = 0; i < 10; ++i)
    testLong();

shouldThrow(() => "7".padStart(3, Symbol()), TypeError);
shouldThrow(() => "7".padEnd(3, thrower), RangeError);
shouldThrow(() => "7".padEnd(2000, thrower), RangeError);
