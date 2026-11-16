function baz() { }
noInline(baz);

function bar(x, y, z) {
    baz(z);
    return x + y + 20.2;
}
function foo(x, b) {
    let y = x + 10.54;
    let z = y;
    if (b) {
        y += 1.23;
        z += 2.199;
    } else {
        y += 2.27;
        z += 2.18;
    }

    let r = bar(b ? y : z, !b ? y : z);

    return r;
}
noInline(foo);

for (let i = 0; i < 1000; ++i)
    foo(i+0.5, !!(i%2));
