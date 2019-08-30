//@ runDefault("--jitPolicyScale=0", "--useCodeCache=0")
let code = `
function* g1() {}
function* g2() {}
g1().next();
g2().next();
`;

// Increasing to 1e5 improves reproducibility.
for (var i=0; i < 1e3; i++)
    runString(code);
