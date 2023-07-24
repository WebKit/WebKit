(async function () {
    let bytes = read('./resources/b3-signed-extend-16-to-64.wasm', 'binary');
    let i0 = await WebAssembly.instantiate(bytes);
    let { fn, global } = i0.instance.exports;
    for (let k = 0; k < 10000; k++) {
        fn();
    }
})().catch(debug);
