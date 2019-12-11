function assert(b) {
    if (!b)
        throw new Error;
}
noInline(assert);

function foo(a, b) {
    let r1 = b[0];
    let x = [...a];
    let r2 = b[0];
    assert(r1 + r2 === 43);
}
noInline(foo);

let b = [42];
let a = [];
a[Symbol.iterator] = function* () {
    b[0] = 1;
};
for (let i = 0; i < 10000; ++i) {
    b[0] = 42;
    foo(a, b);
}
