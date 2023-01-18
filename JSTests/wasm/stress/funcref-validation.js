(async function () {
  let bytes = readFile('./resources/funcref-validation.wasm', 'binary');
  let importObject = {
    m: {
      f0: new WebAssembly.Global({value: 'f32', mutable: true}, 0),
      f1: new WebAssembly.Global({value: 'f64', mutable: true}, 0),
      f2: 0,
      f3: new WebAssembly.Global({value: 'f64', mutable: true}, 0),
      f4: new WebAssembly.Global({value: 'i64', mutable: true}, 0n),
      f5: 0,
      f6: new WebAssembly.Global({value: 'anyfunc', mutable: true}, null),
      f7: undefined,
      ifn0() {},
      ifn1() {},
      ifn2() {},
      t0: new WebAssembly.Table({initial: 61, element: 'anyfunc'}),
      t1: new WebAssembly.Table({initial: 16, element: 'externref'}),
    },
  };
  let i = await WebAssembly.instantiate(bytes, importObject);
  i.instance.exports.fn3();
})();
