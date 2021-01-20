function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

var isUndefinedOrNull = $vm.createBuiltin(`(function (value) { return @isUndefinedOrNull(value); })`);
noInline(isUndefinedOrNull);

var masquerader = makeMasquerader();
for (var i = 0; i < 1e5; ++i) {
    shouldBe(isUndefinedOrNull(null), true);
    shouldBe(isUndefinedOrNull(undefined), true);
    shouldBe(isUndefinedOrNull("Hello"), false);
    shouldBe(isUndefinedOrNull(Symbol("Hello")), false);
    shouldBe(isUndefinedOrNull(42), false);
    shouldBe(isUndefinedOrNull(-42), false);
    shouldBe(isUndefinedOrNull(0), false);
    shouldBe(isUndefinedOrNull(-0), false);
    shouldBe(isUndefinedOrNull(42.2), false);
    shouldBe(isUndefinedOrNull(-42.2), false);
    shouldBe(isUndefinedOrNull({}), false);
    shouldBe(isUndefinedOrNull([]), false);
    shouldBe(isUndefinedOrNull(true), false);
    shouldBe(isUndefinedOrNull(masquerader), false);
}
