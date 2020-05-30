// This should not crash.
function foo() {
    const o = { y: 0 };
    delete o.y;
    o.z = 0;
    Object.assign({}, o);
}
noInline(foo);

for (let i = 0; i < 100000; i++) {
    foo();
}
