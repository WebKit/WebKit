//@ requireOptions("--jitPolicyScale=0", "--validateFTLOSRExitLiveness=1")

function foo() {
  for (let i=0; i<100; i++) {
    let u8 = new Uint8Array(4);
    for (let q of u8) {}
    let u32 = new Uint32Array(2);
    for (let q of u32) {}
  }
}

for (let i=0; i<100; i++) {
  foo();
}
