//@ runDefault("--slowPathAllocsBetweenGCs=4", "--watchdog=100", "--watchdog-exception-ok")

function foo() {
  let { ...r } = { xx:0 };
  foo();
}

try {
    foo();
} catch { }
