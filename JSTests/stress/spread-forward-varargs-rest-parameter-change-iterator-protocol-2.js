function assert(b, m="") {
    if (!b)
        throw new Error("Bad assertion: " + m);
}
noInline(assert);

let called = false;
function baz(c) {
    if (c) {
        Array.prototype[Symbol.iterator] = function() {
            assert(false, "should not be called!");
            let i = 0;
            return {
                next() {
                    assert(false, "should not be called!");
                }
            };
        }
    }
}
noInline(baz);

function bar(...args) {
    return args;
}
noInline(bar);

function foo(c, ...args) {
    args = [...args];
    baz(c);
    return bar.apply(null, args);
}
noInline(foo);

function test(c) {
    const args = [{}, 20, [], 45];
    let r = foo(c, ...args);
    assert(r.length === r.length);
    for (let i = 0; i < r.length; i++)
        assert(r[i] === args[i]);
}
noInline(test);
for (let i = 0; i < 40000; i++) {
    test(false);
}
test(true);
