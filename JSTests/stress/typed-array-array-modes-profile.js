function foo(o) {
    for (var i = 0; i < 100; ++i) {
        o.f = o.f;
    }
}

let typedArrays = [
    Uint8Array,
    Uint32Array,
    Uint8Array,
];

for (let constructor of typedArrays) {
    let a = new constructor(0);
    for (let i = 0; i < 10000; i++) {
        foo(a);
    }
}
