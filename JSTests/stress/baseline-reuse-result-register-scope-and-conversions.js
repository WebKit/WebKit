function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected " + expected);
}

function shouldThrowReferenceError(func) {
    let threw = false;
    try {
        func();
    } catch (error) {
        threw = error instanceof ReferenceError;
    }
    if (!threw)
        throw new Error("expected a ReferenceError");
}

function templateOfSum(a, b) {
    return `${a + b}`;
}
noInline(templateOfSum);

function postIncrementInPlace(x) {
    x = x++;
    return x + x;
}
noInline(postIncrementInPlace);

function writeLateLexical(a, b) {
    lateLexical = a + b;
}
noInline(writeLateLexical);

globalThis.globalPropertyTarget = 0;
function writeGlobalProperty(a, b) {
    globalPropertyTarget = a + b;
    return globalPropertyTarget;
}
noInline(writeGlobalProperty);

function makeInjectable() {
    var captured = 0;
    function makeWriters() {
        eval("");
        function writeSum(a, b) {
            captured = a + b;
            return captured;
        }
        function writeArgument(a) {
            captured = a;
            return captured;
        }
        function inject(code) {
            eval(code);
        }
        return { writeSum, writeArgument, inject };
    }
    return makeWriters();
}
var injectable = makeInjectable();
noInline(injectable.writeSum);
noInline(injectable.writeArgument);

for (var i = 0; i < testLoopCount / 10; ++i)
    shouldThrowReferenceError(() => writeLateLexical(1, 2));

let lateLexical = 0;

for (var i = 0; i < testLoopCount * 2; ++i) {
    shouldBe(templateOfSum(1, 2), "3");
    shouldBe(postIncrementInPlace(4), 8);
    writeLateLexical(i, 1);
    shouldBe(lateLexical, i + 1);
    shouldBe(writeGlobalProperty(i, 2), i + 2);
    shouldBe(injectable.writeSum(i, 3), i + 3);
    shouldBe(injectable.writeArgument(i), i);
    if (!(i % 97)) {
        shouldBe(templateOfSum("a", 2), "a2");
        shouldBe(templateOfSum({ toString() { return "x"; }, valueOf: undefined }, ""), "x");
        shouldBe(postIncrementInPlace("4"), 8);
        shouldBe(postIncrementInPlace(4n), 8n);
        writeLateLexical("s", 1);
        shouldBe(lateLexical, "s1");
        shouldBe(writeGlobalProperty("g", 2), "g2");
        shouldBe(injectable.writeSum("c", 3), "c3");
        shouldBe(injectable.writeArgument("v"), "v");
    }
    if (i === testLoopCount) {
        injectable.inject("var injectedVariable" + i + " = 1");
        delete globalThis.globalPropertyTarget;
        globalThis.globalPropertyTarget = 0;
    }
}
