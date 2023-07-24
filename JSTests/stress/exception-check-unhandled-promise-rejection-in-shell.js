//@ runDefault("--watchdog=100", "--watchdog-exception-ok")
async function foo() {
  await undefined;
  throw new Error();
}

setUnhandledRejectionCallback(foo);
foo();
drainMicrotasks();
