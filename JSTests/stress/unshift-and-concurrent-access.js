//@ runDefault("--jitPolicyScale=0", "--watchdog-exception-ok", "--watchdog=100")
let a0 = [];
for (let j = 0; j < 1000; j++) {
  for (let i = 0; i < 10000; i++) {
    a0.unshift(0);
  }
  Array.prototype.__defineGetter__('a', () => {});
  a0.x++;
}
