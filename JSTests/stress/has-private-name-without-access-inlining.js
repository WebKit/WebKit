//@ requireOptions("--useAccessInlining=false")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + String(actual) + ", expected: " + String(expected));
}

class Base {
    constructor(object) {
        return object;
    }
}

class Stamper extends Base {
    #field;
    static has(object) {
        return #field in object;
    }
}
noInline(Stamper.has);

let prototype = { };
let child = Object.create(prototype);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(Stamper.has(child), false);

new Stamper(prototype);
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(Stamper.has(child), false);
    shouldBe(Stamper.has(prototype), true);
}
