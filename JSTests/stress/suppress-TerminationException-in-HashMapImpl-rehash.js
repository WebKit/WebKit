//@ runDefault("--watchdog=100", "--watchdog-exception-ok")

for (let i=0; i<1000; i++) {
  new Set('abcdefghijklmnopqrstuvwxyz');
}
