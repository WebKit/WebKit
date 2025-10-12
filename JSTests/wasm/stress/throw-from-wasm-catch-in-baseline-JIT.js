//@ runNoisyTestDefault("--traceBaselineJITExecution=1", "--jitPolicyScale=0")

let x = readFile('throw-from-wasm-catch-in-baseline-JIT-payload.bin', 'binary');
let module = new WebAssembly.Module(x);
let instance = new WebAssembly.Instance(module);

function foo(instance) {
  try {
    instance.exports['']();
  } catch {}
}

for (let i = 0; i < 1000; i++)
    foo(instance);

