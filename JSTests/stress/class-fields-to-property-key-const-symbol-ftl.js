//@ requireOptions("--useClassFields=true")
//@ if isFTLEnabled then runFTLNoCJIT else skip end

let ftlTrue = $vm.ftlTrue;
let didFTLCompile = false;
let symbol = Symbol("test");
function constFoldSymbol(i) {
    class C {
        [symbol] = i;
    }
    didFTLCompile = ftlTrue();
    let c = new C();
    if (c[symbol] !== i)
        throw new Error(`Failed on iteration ${i}\n${JSON.stringify(c)}`);
}
noInline(constFoldSymbol);

let i = 0;
let maxTries = 10000;
for (; i < maxTries && !numberOfDFGCompiles(constFoldSymbol) && !didFTLCompile; ++i) {
    optimizeNextInvocation(constFoldSymbol);
    constFoldSymbol(i);
}

if (i >= maxTries)
    throw new Error("Failed to compile with DFG JIT");
