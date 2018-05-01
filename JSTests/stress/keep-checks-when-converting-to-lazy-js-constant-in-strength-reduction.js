//@ runDefault("--jitPolicyScale=0", "--useConcurrentJIT=false")

let bar;
for (let i = 0; i < 20; ++i) {
  bar = i ** 0;
  bar + '';
}
