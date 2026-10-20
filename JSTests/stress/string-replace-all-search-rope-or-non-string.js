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
let log = "";
let search = {
    toString() {
        log += "s";
        return "o";
    }
};
let replacement = {
    toString() {
        log += "r";
        return "0";
    }
};
let thrower = {
    toString() {
        throw new RangeError("thrower");
    }
};

function offset(match, offset) {
    return offset;
}

function test(i) {
    shouldBe("fabab".replaceAll(rope("a", "b"), "o"), "foo");
    shouldBe("fabab".replaceAll(substring(base, 10, 12), "o"), "foo");
    shouldBe("fabab".replace(substring(base, 10, 12), "o"), "foab");
    shouldBe("a1b1".replaceAll(1, "x"), "axbx");
    shouldBe("a12b13".replaceAll(12 + (i & 1), "x"), (i & 1) ? "a12bx" : "axb13");
    shouldBe("a1.5b1.5".replaceAll(1.5, "x"), "axbx");
    shouldBe("anullbnull".replaceAll(null, "x"), "axbx");
    shouldBe("aundefinedb".replaceAll(undefined, "x"), "axb");
    shouldBe("atruebtrue".replaceAll(true, "x"), "axbx");
    shouldBe("foo".replaceAll(rope("", ""), "-"), "-f-o-o-");
    shouldBe("a1b1".replaceAll(1, offset), "a1b3");

    log = "";
    shouldBe("foo".replaceAll(search, replacement), "f00");
    shouldBe(log, "sr");
    shouldBe("foo".replace(search, replacement), "f0o");
    shouldBe(log, "srsr");
    shouldBe("fxx".replaceAll(search, "0"), "fxx");
    shouldBe(log, "srsrs");
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    test(i);

shouldThrow(() => "foo".replaceAll(Symbol(), "x"), TypeError);
shouldThrow(() => "foo".replaceAll(thrower, "x"), RangeError);
shouldThrow(() => "foo".replaceAll(thrower, replacement), RangeError);
