//@ runNoisyTestDefault("--destroy-vm", "--forceICFailure=1", "--useICStats=1", "--useDFGJIT=0")
for (let i = 0; i < 1000000; i++) {
  ({a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0, h: 0, i: 0, j: 0, k: 0, l: 0});
}
