//@ runDefault("--forceDebuggerBytecodeGeneration=1", "--useOSRExitFuzz=1", "--watchdog=500", "--watchdog-exception-ok", "--verboseOSRExitFuzz=0")
function foo(a0) {
  while (1) {}
}

foo(0);
