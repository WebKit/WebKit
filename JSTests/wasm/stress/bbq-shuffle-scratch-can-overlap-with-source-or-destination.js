(async function () {
  let bytes = read('./resources/bbq-shuffle-scratch-can-overlap-with-source-or-destination.wasm', 'binary');
  let i0 = await WebAssembly.instantiate(bytes);
  let {fn} = i0.instance.exports;
  for (let k = 0; k < 10000; k++) {
    let xs = fn(-3);
    -xs[5];
  }
})().catch(debug);
