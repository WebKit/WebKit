let bytes = readFile('./resources/tuple-and-v128.wasm', 'binary');
let importObject = {
  m: {
    ifn0: () => { },
    f0: new WebAssembly.Global({value: 'f32', mutable: false}, 0),
    f1: null,
  },
};
(async function () {
  let i = await WebAssembly.instantiate(bytes, importObject);
  for (let j = 0; j < 100; j++) {
    try { debug(i.instance.exports.fn4(0n)); } catch (e) { debug(e); }
  }
})();
