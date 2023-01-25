let bytes = readFile('./resources/funcref-race.wasm', 'binary');
(async function () {
    let importObject = {
        m: {
            ifn0() { },
            ifn1() { },
            ifn2() { },
            t0: new WebAssembly.Table({ initial: 18, element: 'externref' }),
            t1: new WebAssembly.Table({ initial: 84, element: 'anyfunc' }),
            t2: new WebAssembly.Table({ initial: 2, element: 'externref' }),
            t3: new WebAssembly.Table({ initial: 6, element: 'anyfunc' }),
            t4: new WebAssembly.Table({ initial: 67, element: 'anyfunc', maximum: 579 }),
            t5: new WebAssembly.Table({ initial: 39, element: 'externref', maximum: 690 }),
        },
    };
    for (let j = 0; j < 10000; j++) {
        try {
            let i = await WebAssembly.instantiate(bytes, importObject);
            i.instance.exports.fn10();
        } catch (e) {
        }
    }
})();
