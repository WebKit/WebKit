//@ requireOptions("--watchdog=1000", "--watchdog-exception-ok")
// This test only seems to reproduce the issue when it runs in an infinite loop. So we use the watchdog to time it out.
for (let x in [0]) {
  break
}
while(1);
