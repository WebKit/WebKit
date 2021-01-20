function assert(b) {
    if (!b)
        throw new Error;
}

function foo(view) {
    let x = view.getFloat64(0);
    return [x, x | 0];
}
noInline(foo);

let buffer = new ArrayBuffer(8);
let view = new DataView(buffer);
for (let i = 0; i < 1000000; ++i) {
    for (let i = 0; i < 8; ++i) {
        view.setInt8(i, Math.random() * 255);
    }

    let [a, b] = foo(view);
    if (isNaN(a))
        assert(b === 0);
}
