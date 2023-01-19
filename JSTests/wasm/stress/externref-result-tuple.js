//@ skip if $architecture == "arm"
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(async function () {
  let bytes = readFile('./resources/externref-result-tuple.wasm', 'binary');
  let importObject = {
    m: {
      f0: 0, f1: null, f2: 0n, f3: 0, f4: null,
      f5: new WebAssembly.Global({value: 'externref', mutable: true}, {}),
      ifn0() { return [0, 0, 0n]; },
      ifn1() {},
      ifn2() { return [undefined, null, 0, 0, undefined]; },
      ifn3() { return [undefined, null, 0, 0, undefined]; },
      t0: new WebAssembly.Table({initial: 49, element: 'anyfunc', maximum: 687}),
      t1: new WebAssembly.Table({initial: 22, element: 'externref'}),
      t2: new WebAssembly.Table({initial: 97, element: 'externref', maximum: 699}),
    },
    mem: {ory: new WebAssembly.Memory({initial: 40241, shared: true, maximum: 65536})},
  };
  let i = await WebAssembly.instantiate(bytes, importObject);
  try { i.instance.exports.fn4(); } catch (e) { }
  let table = i.instance.exports.table3;
  shouldBe(table.get(0), undefined);
})().catch($vm.abort);
