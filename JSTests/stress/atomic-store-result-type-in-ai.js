//@ runDefault("--forceDebuggerBytecodeGeneration=1", "--useConcurrentJIT=0")
let a = new Uint8Array(1);
for (let i = 0; i < 10000; ++i) {
  Atomics.store(a, 0, i);
}
