//@ skip if $memoryLimited
//@ skip if $buildType == "debug"
//@ runDefault("--destroy-vm", "--forceDebuggerBytecodeGeneration=1", "--returnEarlyFromInfiniteLoopsForFuzzing=1")

function useAllMemory() {
  const a = [0];
  a.__proto__ = {};
  Object.defineProperty(a, 0, {get: foo});
  Object.defineProperty(a, 80000000, {});

  function foo() {
    new Uint8Array(a);
  }

  new Promise(foo);
  try {
    for (let i = 0; i < 2**32; i++) {
      new ArrayBuffer(1000);
    }
  } catch {
  }
}

try {
    useAllMemory();

    function bar(a0) {
      arguments;
    }
    bar();
} catch { }
