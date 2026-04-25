//@ requireOptions("--useWasmSIMD=1")
//@ skip unless $isSIMDPlatform
(async function () {
  let bytes0 = readFile('./resources/call-returns-v128.wasm', 'binary');
  let fn0 = function () {};
  let fn1 = fn0;
  let fn2 = fn0;
  let fn3 = fn0;
  let fn4 = fn0;
  let fn5 = fn0;
  let fn6 = fn0;
  let fn7 = fn0;
  let fn8 = fn0;
  let global0 = new WebAssembly.Global({value: 'externref', mutable: true}, {});
  let global1 = new WebAssembly.Global({value: 'i32', mutable: true}, 0);
  let global4 = new WebAssembly.Global({value: 'anyfunc', mutable: true}, null);
  let global5 = new WebAssembly.Global({value: 'anyfunc', mutable: true}, null);
  let memory0 = new WebAssembly.Memory({initial: 3325, shared: true, maximum: 5196});
  let table0 = new WebAssembly.Table({initial: 36, element: 'externref', maximum: 505});
  let table3 = new WebAssembly.Table({initial: 93, element: 'anyfunc', maximum: 465});
  let table4 = new WebAssembly.Table({initial: 91, element: 'externref', maximum: 564});
  let table6 = new WebAssembly.Table({initial: 41, element: 'anyfunc', maximum: 41});
  let m0 = {fn3, fn4, fn6, fn7, global0, global5, memory0, table2: table0, table5: table3, table6};
  let m1 = {fn2, global1, table0, table1: table0};
  let m2 = {fn0, fn1, fn5, fn8, global2: global1, global3: 0, global4, table3, table4};
  let importObject0 = {m0, m1, m2};
  let i0 = await WebAssembly.instantiate(bytes0, importObject0);
  let {fn27} = i0.instance.exports;
  fn27();
})().catch($vm.abort);
