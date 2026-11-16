function assertNotUndefined(x) {
    if (x === undefined)
        throw new Error("Bad assertion!");
}

let weakRefs = [];
let functions = [];
for (let i = 0; i < 1000; ++i) {
    let o = { i };
    weakRefs.push(new WeakRef(o));
    functions.push(createNoopNativeFunctionWithCapture(o));
}
setTimeout(() => {
    gc();
    for (let weakRef of weakRefs) {
        assertNotUndefined(weakRef.deref());
    }
});