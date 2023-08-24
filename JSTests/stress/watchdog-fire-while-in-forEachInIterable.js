//@ skip if $memoryLimited
//@ runDefault("--watchdog=100", "--watchdog-exception-ok")

const a = [];
a[2**32-2] = 0;
new Uint8Array(a);
