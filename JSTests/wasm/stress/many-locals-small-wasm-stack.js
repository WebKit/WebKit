//@ skip
// This is an infinite loop, so untill watchdog works we have to skip it.
(async function () {
  let bytes = readFile('many-locals-small-wasm-stack.wasm', 'binary');
  let importObject = { };
  let i = await WebAssembly.instantiate(bytes, importObject);
  i.instance.exports.main(0n, 0);
})();
