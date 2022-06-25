//@ requireOptions("--watchdog=20", "--jitPolicyScale=0", "--watchdog-exception-ok")

// This test should not time out.
while (1) { }
