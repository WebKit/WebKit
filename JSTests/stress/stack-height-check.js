//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0.5")

function test(value) {
}
noInline(test);

const v19 = [11, 22, 33, 44, 55, 66, 77];
for (const v20 in v19) {
  const v24 = new Proxy({}, {});
  v19.__proto__ = v24;
  test(v20)
  for(let i=0; i<200; i++){}
}
