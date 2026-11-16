//@ requireOptions("--exposeCustomSettersOnGlobalObjectForTesting=true")

function assert(b) {
    if (!b)
        throw new Error;
}

let global = this;
Object.defineProperty(global, "Y", {
    set: function(v) {
        assert(this === global);
        assert(v === i + 1);
        this._Y = v;
    }
});

function foo(x, y, z, a) {
    this.X = x;
    this.Y = y;
    this.testCustomAccessorSetter = z;
    this.testCustomValueSetter = a;
}
noInline(foo);

let i;
for (i = 0; i < 1000000; ++i) {
    foo(i, i + 1, i + 2, i + 3);
    assert(global.X === i);
    assert(global._Y === i + 1);
    assert(global._testCustomAccessorSetter === i + 2);
    assert(global._testCustomValueSetter === i + 3);
}
