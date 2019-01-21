function foo(view) {
    return view.setInt8(0, 0);
}
noInline(foo);

let a = new Int8Array(10);
let dataView = new DataView(a.buffer);
for (let i = 0; i < 10000; ++i) {
    if (foo(dataView) !== undefined)
        throw new Error("Bad!")
}
