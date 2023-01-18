//@ skip if $architecture == "arm"
(async function () {
    /*
(module
  (global (;0;) v128 v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000)
  (export "global" (global 0))
)
*/
  let bytes = readFile('./resources/simd-global.wasm', 'binary');
  let x = await WebAssembly.instantiate(bytes);
})().catch(e => debug(e));
