//@ requireOptions("--useClassFields=true")
//@ if isFTLEnabled then runFTLNoCJIT else skip end

let ftlTrue = $vm.ftlTrue;
let didFTLCompile = false;
function stringObjectToString(i) {
    class C {
        [new String("foo")] = i;
    }
    didFTLCompile = ftlTrue();
    let c = new C();
    if (c.foo !== i)
        throw new Error(`Failed on iteration ${i}\n${JSON.stringify(c)}`);
}
noInline(stringObjectToString);

let i = 0;
let maxTries = 30000;
for (; i < maxTries && !numberOfDFGCompiles(stringObjectToString) && !didFTLCompile; ++i) {
    optimizeNextInvocation(stringObjectToString);
    stringObjectToString(i);
}

if (i >= maxTries)
    throw new Error("Failed to compile stringObjectToString with DFG JIT");
