function assert(b) {
    if (!b)
        throw new Error;
}

function bar(a, x) {
    return a[x];
}
noInline(bar);

let o = {};
let b = [o, "", , 25];

for (let i = 0; i < 10000000; ++i) {
    assert(bar(b, 0) === o);
    assert(bar(b, 5) === undefined);
    assert(bar(b, 2) === undefined);
}
