function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

function makeState() {
    var state = {};
    for (var i = 0; i < 8; ++i)
        state["k" + i] = i;
    return state;
}

for (var i = 0; i < testLoopCount; ++i) {
    var state = makeState();
    var result = Object.assign({}, state, { k3: -1, extra: i });
    shouldBeArray(Object.keys(result), ["k0", "k1", "k2", "k3", "k4", "k5", "k6", "k7", "extra"]);
    shouldBe(result.k3, -1);
    shouldBe(result.k7, 7);
    shouldBe(result.extra, i);
    shouldBe(state.k3, 3);
    result.k0 = 42;
    shouldBe(state.k0, 0);

    var empty = Object.assign({}, state, {});
    shouldBeArray(Object.keys(empty), Object.keys(state));
    shouldBe(Object.getPrototypeOf(empty), Object.prototype);
    shouldBe(Object.isFrozen(empty), false);

    var three = Object.assign({}, { a: 1 }, { b: 2 }, { a: 3 });
    shouldBeArray(Object.keys(three), ["a", "b"]);
    shouldBe(three.a, 3);

    var frozen = Object.freeze(makeState());
    var fromFrozen = Object.assign({}, frozen, { k1: "x" });
    shouldBe(Object.isFrozen(fromFrozen), false);
    fromFrozen.k0 = "y";
    shouldBe(fromFrozen.k0, "y");
    shouldBe(fromFrozen.k1, "x");
    shouldBe(frozen.k1, 1);

    var sym = Symbol("s");
    var withSymbol = makeState();
    withSymbol[sym] = "sym";
    Object.defineProperty(withSymbol, "hidden", { value: 1, enumerable: false });
    var copied = Object.assign({}, withSymbol, { k2: 2 });
    shouldBe(copied[sym], "sym");
    shouldBe(Object.getOwnPropertyDescriptor(copied, "hidden"), undefined);

    var nonEmptyTarget = Object.assign({ first: 0 }, state, {});
    shouldBe(Object.keys(nonEmptyTarget)[0], "first");
    shouldBe(Object.keys(nonEmptyTarget).length, 9);
}
