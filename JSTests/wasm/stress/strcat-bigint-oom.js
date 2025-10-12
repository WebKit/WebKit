//@ skip if $memoryLimited
//@ runDefault("--watchdog=1000", "--watchdog-exception-ok")
let b = 2n ** 1000n;

function foo() {
  let m = new WebAssembly.Memory({initial: 4096});
  try {
    foo();
  } catch {}
  try {
    for (let i = 0; i < testLoopCount; i++) {
      new Uint8Array(i);
    }
    x;
  } catch {}
  try {
    b + '' + '';
  } catch {}
}

for (let i = 0; i < 100; i++) {
  foo();
}
