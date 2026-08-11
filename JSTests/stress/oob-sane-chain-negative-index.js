let called = false;
Object.defineProperty(Array.prototype, "-1", {
    get: function() {
        called = true;
        return 42;
    }
});
function assert(b) {
    if (!b)
        throw new Error;
}

function baz(a, x) {
    return a[x];
}
noInline(baz);

let a = [1,2,3];

for (let i = 0; i < 10000000; ++i) {
    assert(baz(a, -1) === 42);
    assert(called);
    called = false;
}
