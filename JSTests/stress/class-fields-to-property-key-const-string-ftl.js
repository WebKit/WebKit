//@ requireOptions("--useClassFields=true")
//@ if isFTLEnabled then runFTLNoCJIT else skip end

let ftlTrue = $vm.ftlTrue;
let didFTLCompile = false;
function constFoldString(i) {
    class C {
        ["foo"] = i;
    }
    didFTLCompile = ftlTrue();
    let c = new C();
    if (c.foo !== i)
        throw new Error(`Failed on iteration ${i}\n${JSON.stringify(c)}`);
}
noInline(constFoldString);

let i = 0;
let maxTries = 10000;
for (; i < maxTries && !numberOfDFGCompiles(constFoldString) && !didFTLCompile; ++i) {
    optimizeNextInvocation(constFoldString);
    constFoldString(i);
}

if (i >= maxTries)
    throw new Error("Failed to compile constFoldString with DFG JIT");
