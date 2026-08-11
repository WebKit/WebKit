//@ runDefault("--watchdog=100", "--watchdog-exception-ok")
async function infiniteLoop() {
  await undefined;
  while (1) ;
}

infiniteLoop();
drainMicrotasks();
[].slice();
