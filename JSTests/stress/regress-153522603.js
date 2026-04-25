//@ runDefault("--useFTLJIT=0", "--jitPolicyScale=0.1", "--watchdog=200", "--watchdog-exception-ok")

for (;;) {
  edenGC();
  var x = ['xy'].indexOf('xy_'.substring(0, 2));
}
