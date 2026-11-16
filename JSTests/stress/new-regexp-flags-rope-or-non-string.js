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

let flagsBase = "xgimyz";
let count = 0;
let object = {
    toString() {
        ++count;
        return "gi";
    }
};
let thrower = {
    toString() {
        throw new RangeError("thrower");
    }
};

function create(pattern, flags) {
    return new RegExp(pattern, flags);
}
noInline(create);

function createOrThrow(flags) {
    try {
        return new RegExp("a", flags).flags;
    } catch (error) {
        return error.constructor;
    }
}
noInline(createOrThrow);

function test(i) {
    shouldBe(create("a", rope("g", "i")).flags, "gi");
    shouldBe(create("a", substring(flagsBase, 1, 4)).flags, "gim");
    shouldBe(create("a", rope("", "")).flags, "");
    shouldBe(create(/a/g, rope("i", "m")).flags, "im");
    shouldBe(RegExp("a", rope("g", "y")).flags, "gy");

    let before = count;
    shouldBe(create("a", object).flags, "gi");
    shouldBe(count, before + 1);

    shouldBe(createOrThrow((i & 63) ? "g" : 1), (i & 63) ? "g" : SyntaxError);
    shouldBe(createOrThrow((i & 63) ? rope("g", "i") : rope("g", "g")), (i & 63) ? "gi" : SyntaxError);
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    test(i);

shouldThrow(() => new RegExp("a", null), SyntaxError);
shouldThrow(() => new RegExp("a", true), SyntaxError);
shouldThrow(() => new RegExp("a", 1.5), SyntaxError);
shouldThrow(() => new RegExp("a", Symbol()), TypeError);
shouldThrow(() => new RegExp("a", thrower), RangeError);
