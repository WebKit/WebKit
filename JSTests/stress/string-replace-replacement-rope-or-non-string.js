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

function offset(match, offset) {
    return offset;
}

function mixed(match, offset) {
    if (offset === 1)
        return rope("a", "b");
    return object;
}

function test(i) {
    shouldBe("foo".replace("o", rope("a", "b")), "fabo");
    shouldBe("foo".replaceAll("o", rope("a", "b")), "fabab");
    shouldBe("foo".replace("o", rope("[$", "&]")), "f[o]o");
    shouldBe("foo".replaceAll("o", rope("[$", "&]")), "f[o][o]");
    shouldBe("foo".replace("o", substring(base, 10, 12)), "fabo");
    shouldBe("foo".replaceAll("o", substring(base, 10, 12)), "fabab");
    shouldBe("foo".replace("o", 5), "f5o");
    shouldBe("foo".replaceAll("o", 12 + (i & 1)), (i & 1) ? "f1313" : "f1212");
    shouldBe("foo".replace("o", 1.5), "f1.5o");
    shouldBe("foo".replaceAll("o", null), "fnullnull");
    shouldBe("foo".replace("o", undefined), "fundefinedo");
    shouldBe("foo".replaceAll("o"), "fundefinedundefined");
    shouldBe("foo".replaceAll("o", ""), "f");
    shouldBe("foo".replace("x", rope("a", "b")), "foo");
    shouldBe("foo".replaceAll("x", 5), "foo");
    shouldBe(rope("fo", "o").replaceAll(rope("o", "o"), rope("a", "b")), "fab");

    let before = count;
    shouldBe("foo".replaceAll("o", object), "fxyxy");
    shouldBe(count, before + 1);
    shouldBe("foo".replace("x", object), "foo");
    shouldBe(count, before + 2);

    shouldBe("foo".replace("o", offset), "f1o");
    shouldBe("foo".replaceAll("o", offset), "f12");
    shouldBe("foo".replaceAll("o", String), "foo");
    shouldBe("foo".replaceAll("o", mixed), "fabxy");
    shouldBe(count, before + 3);
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    test(i);

shouldThrow(() => "foo".replace("o", Symbol()), TypeError);
shouldThrow(() => "foo".replaceAll("o", thrower), RangeError);
shouldThrow(() => "foo".replaceAll("o", () => thrower), RangeError);
shouldThrow(() => "foo".replace("x", thrower), RangeError);
