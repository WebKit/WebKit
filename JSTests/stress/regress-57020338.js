//@ runDefault("--maximumOptimizationDelay=0")
let code = `
let z = {};
function foo() {
  const x = undefined * [];
  for (let j = 0; j < 50000; j++) {
    const y = x * 0;
    z[{}] = y;
  }
}
for (let i = 0; i < 3; i++) {
  foo();
}
`;

for (let i=0; i<35; i++) {
  runString(code);
}
