function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + String(actual) + " expected: " + String(expected));
}

function shouldThrow(func, errorType, message) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error("bad error: " + String(error));
    if (message !== undefined)
        shouldBe(String(error), message);
}

function testAnonymousClass() {
    let C = class { };
    shouldBe((class { }).name, "");
    shouldBe(typeof new (class { })(), "object");
    shouldBe(new C() instanceof C, true);
    let D = class extends C { };
    shouldBe(new D() instanceof C, true);
}

function testEmptyNamedFunction() {
    shouldBe(({ "": function () { } })[""].name, "");
    shouldBe(({ ""() { } })[""].name, "");
    shouldBe(({ [""]: () => { } })[""].name, "");
}

function testComputedAndNumericClassElements() {
    let k = "field";
    let C = class {
        static [Symbol.iterator]() { return 1; }
        [k] = 1;
        0x10() { return 16; }
        1n() { return 1; }
        static 2.5() { return 2.5; }
    };
    shouldBe(C[Symbol.iterator](), 1);
    let c = new C();
    shouldBe(c.field, 1);
    shouldBe(c[16](), 16);
    shouldBe(c["1"](), 1);
    shouldBe(C["2.5"](), 2.5);
}

function testLabels() {
    let count = 0;
    l: for (;;) {
        count++;
        break l;
    }
    shouldBe(count, 1);

    count = 0;
    outer: for (let i = 0; i < 3; ++i) {
        block: {
            for (let j = 0; j < 3; ++j) {
                if (j === 1)
                    continue outer;
                if (j === 2)
                    break block;
                count++;
            }
        }
        for (;;)
            break;
        do {
            count += 10;
            continue;
        } while (false);
    }
    shouldBe(count, 3);
}

function testInvalidEscapeTemplate() {
    function tag(strings) {
        return strings;
    }
    let strings = tag`\u{`;
    shouldBe(strings[0], undefined);
    shouldBe(strings.raw[0], "\\u{");
    strings = tag`a${0}\xg`;
    shouldBe(strings[0], "a");
    shouldBe(strings[1], undefined);
    shouldBe(strings.raw[1], "\\xg");
}

function testStaticErrorMessages() {
    function destructureTwice(a, b) {
        let { x } = a;
        let { x: y } = b;
        return x + y;
    }
    shouldThrow(() => destructureTwice(null, { x: 1 }), TypeError);
    shouldThrow(() => destructureTwice({ x: 1 }, undefined), TypeError);
    shouldBe(destructureTwice({ x: 1 }, { x: 2 }), 3);

    class C { }
    shouldThrow(() => C(), TypeError);
    shouldThrow(() => C(), TypeError);
}

for (let i = 0; i < testLoopCount / 10; ++i) {
    testAnonymousClass();
    testEmptyNamedFunction();
    testComputedAndNumericClassElements();
    testLabels();
    testInvalidEscapeTemplate();
    testStaticErrorMessages();
}
