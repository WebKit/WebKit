//@ skip if ["powerpc64", "s390"].include?($architecture)
//@ runDefault("--slowPathAllocsBetweenGCs=4", "--watchdog=100", "--watchdog-exception-ok")

function foo() {
  let { ...r } = { xx:0 };
  foo();
}

try {
    foo();
} catch { }
