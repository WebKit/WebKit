// Makes sure we don't use base's tag register on 32-bit when an inline cache fails and jumps to the slow path
// because the slow path depends on the base being present.

function assert(b) {
    if (!b)
        throw new Error("baddd");
}
noInline(assert);

let customGetter = createCustomGetterObject();
let otherObj = {
    customGetter: 20
};
function randomFunction() {}
noInline(randomFunction);

function foo(o, c) {
    let baz  = o.customGetter;
    if (c) {
        o = 42;
    }
    let jaz = o.foo;
    let kaz = jaz + "hey";
    let raz = kaz + "hey";
    let result = o.customGetter;
    randomFunction(!c, baz, jaz, kaz, raz);
    return result;
}
noInline(foo);

for (let i = 0; i < 10000; i++) {
    switch (i % 2) {
    case 0:
        assert(foo(customGetter) === 100);
        break;
    case 1:
        assert(foo(otherObj) === 20);
        break;
    }
}
assert(foo({hello: 20, world:50, customGetter: 40}) === 40); // Make sure we don't trample registers in "o.customGetter" inline cache failure in foo.
