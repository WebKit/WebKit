var createBuiltin = $vm.createBuiltin;

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class Base { }
shouldBe(new Base() instanceof Base, true);

class Derived extends Base { }
shouldBe(new Derived() instanceof Base, true);

{
    let key = {};
    let value = {};
    class DerivedWeakMap extends WeakMap { }
    let map = new DerivedWeakMap([[key, value]]);
    shouldBe(map.get(key), value);
}

{
    class WithField extends Base {
        field = 42;
    }
    shouldBe(new WithField().field, 42);
}

{
    let singleLine = createBuiltin(`(function (a) { return a + 1; })`);
    shouldBe(singleLine(41), 42);
}

{
    let multiLine = createBuiltin(`(function (a) {
    return a + 1;
})
`);
    shouldBe(multiLine(41), 42);
}

{
    let noTrailingNewline = createBuiltin(`(function () { return 'ok'; })`);
    shouldBe(noTrailingNewline(), 'ok');
}
