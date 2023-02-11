(async function () {
  let bytes0 = readFile('./resources/only-referenced.wasm', 'binary');
  let global1 = new WebAssembly.Global({value: 'externref', mutable: true}, {});
  let global2 = new WebAssembly.Global({value: 'i32', mutable: true}, 0);
  let m0 = {fn1() {}, global0: global2};
  let m1 = {global1, global3: 0n};
  let m2 = {fn0() {}, fn2() {}, global2};
  await WebAssembly.instantiate(bytes0, {m0, m1, m2});
})().catch($vm.abort);
