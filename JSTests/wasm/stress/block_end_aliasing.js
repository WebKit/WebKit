//@skip if $memoryLimited
//@ runDefault("--useBBQJIT=0", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeSoon=0", "--thresholdForOMGOptimizeAfterWarmUp=0")
function instantiate(filename, importObject) {
  let bytes = read(filename, 'binary');
  return WebAssembly.instantiate(bytes, importObject);
}

(async function () {
  let global56 = new WebAssembly.Global({value: 'i64', mutable: true}, 0n);
  let global57 = new WebAssembly.Global({value: 'i32', mutable: true}, 0);
  let global58 = new WebAssembly.Global({value: 'externref', mutable: true}, undefined);

  let memory6 = new WebAssembly.Memory({initial: 2546, shared: true, maximum: 3097});

  let tag37 = new WebAssembly.Tag({parameters: ['i32', 'anyfunc']});
  let tag38 = new WebAssembly.Tag({parameters: ['i64']});
  let tag39 = new WebAssembly.Tag({parameters: []});
  let tag40 = new WebAssembly.Tag({parameters: ['f32', 'i64', 'anyfunc']});

  let m12 = {fn40() {}, fn44() { return 0n; }, fn45() {}, fn49() {}, global56, tag39, tag43: tag39};
  let m13 = {fn41() {}, fn43() {}, fn47() {}, global57, memory6, tag38, tag40, tag42: tag39, tag44: tag39};
  let m14 = {fn42() {}, fn46() {}, fn48() {}, global58, tag37, tag41: tag40};
  let importObject4 = {extra: {isJIT: () => true}, m12, m13, m14};
  let i4 = await instantiate('block_end_aliasing.wasm', importObject4);
  let {fn54} = i4.instance.exports;
  let z = fn54(0, 0n, null);
  let bad = z[2];
  bad.x;
}()).catch(() => { });
