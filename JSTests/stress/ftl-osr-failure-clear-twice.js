//@ runDefault("--jitPolicyScale=0", "--osrExitCountForReoptimization=40", "--watchdog=500", "--watchdog-exception-ok")
function foo() {
  const xs = 'x'.repeat(1000);
  for (let x of xs) {
    for (let i=0; i<1000; i++) {}
    try {
      zzz
    } catch {}
  }
}

for (let i=0; i<100; i++) {
  foo();
}
