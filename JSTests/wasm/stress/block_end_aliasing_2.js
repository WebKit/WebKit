//@skip if $memoryLimited
//@ runDefault("--useBBQJIT=0", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeSoon=0", "--thresholdForOMGOptimizeAfterWarmUp=0")
function instantiate(filename, importObject) {
  let bytes = read(filename, 'binary');
  return WebAssembly.instantiate(bytes, importObject);
}

(async function () {
  let i4 = await instantiate('block_end_aliasing_2.wasm', {});
  let {fn54} = i4.instance.exports;
  fn54().x
}()).catch(() => { });
