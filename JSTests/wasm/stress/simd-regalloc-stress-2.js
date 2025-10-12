//@ requireOptions("--useWasmSIMD=1", "--useBBQJIT=1", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeSoon=0")
//@ skip if !$isSIMDPlatform or $memoryLimited
import * as assert from "../assert.js"

function instantiate(filename, importObject) {
    let bytes = read(filename, 'binary');
    return WebAssembly.instantiate(bytes, importObject);
  }

async function test() {
  let memory0 = new WebAssembly.Memory({initial: 0x10, maximum: 0x100, shared: true});
  let global15 = new WebAssembly.Global({value: 'f32', mutable: true}, 0);
  let global16 = new WebAssembly.Global({value: 'anyfunc', mutable: true}, null);
  let global51 = new WebAssembly.Global({value: 'externref', mutable: true}, undefined);
  let global54 = new WebAssembly.Global({value: 'i64', mutable: true}, 0n);
  let table21 = new WebAssembly.Table({initial: 60, element: 'externref', maximum: 60});
  let table22 = new WebAssembly.Table({initial: 61, element: 'anyfunc', maximum: 61});
  let table52 = new WebAssembly.Table({initial: 97, element: 'externref', maximum: 97});
  let table54 = new WebAssembly.Table({initial: 35, element: 'anyfunc', maximum: 35});
  let exportV128 = await instantiate('simd-regalloc-stress-2-export-mutable-v128.wasm', {});
  let {global: globalV128} = exportV128.instance.exports;
  let m6 = {fn13() {}, fn14() {}, fn15() {}, fn19() {}, global15};
  let m7 = {fn18() {}, global16, memory3: memory0, table21, table22};
  let m8 = {fn16() {}, fn17() {}};
  let module0 = await instantiate('simd-regalloc-stress-2-module0.wasm', {extra: {isJIT() {}}, m6, m7, m8});
  let {fn21} = module0.instance.exports;
  let m18 = {
    fn67() {}, fn68() {}, fn69() {}, fn71() {}, fn75() {}, fn76: fn21,
    global50: globalV128, global51, global52: globalV128, global55: global54,
    table51: table22, table54,
  };
  let m19 = {fn65() {}, fn66() {}, fn70() {}, fn73() {}, table53: table21, table55: table21, table56: table52};
  let m20 = {fn64() {}, fn72() {}, fn74() {}, global53: 0, global54, memory8: memory0, table52};
  let module1 = await instantiate('simd-regalloc-stress-2-module1.wasm', {m18, m19, m20});
  let {fn80} = module1.instance.exports;
  try { fn80(null); } catch { }
}

await assert.asyncTest(test())
