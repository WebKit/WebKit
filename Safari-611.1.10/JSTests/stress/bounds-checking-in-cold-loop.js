//@ skip if $memoryLimited
//@ runFTLEager("--watchdog=1000", "--watchdog-exception-ok")
let a = [];
a[0] = undefined;

while (true) {
  let i = 0;
  a[0];
  while ($vm.ftlTrue()) {
    a[i++] = undefined;
  }
}

