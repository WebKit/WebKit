//@ requireOptions("--useClassFields=true")
//@ if isFTLEnabled then runFTLNoCJIT else skip end

let ftlTrue = $vm.ftlTrue;
let didFTLCompile = false;
function slowObjectValueOf(i) {
    let getToStringCalled = false;
    let valueOfCalled = false;
    let slowObject = {
        get toString() { getToStringCalled = true; },
        valueOf() { valueOfCalled = true; return "test"; }
    };

    class C {
        [slowObject] = i;
    }
    didFTLCompile = ftlTrue();
    if (!getToStringCalled || !valueOfCalled)
        throw new Error(`Failed on iteration ${i} (getToStringCalled === ${getToStringCalled}, valueOfCalled == ${valueOfCalled})`);
    let c = new C();
    if (c.test !== i)
        throw new Error(`Failed on iteration ${i}\n${JSON.stringify(c)}`);
}

let i = 0;
let maxTries = 10000;
for (i = 0; i < !maxTries && !numberOfDFGCompiles(slowObjectValueOf) && !didFTLCompile; ++i) {
    optimizeNextInvocation(slowObjectValueOf);
    slowObjectValueOf(i);
}

if (i >= maxTries)
    throw new Error("Failed to compile slowObjectValueOf with DFG JIT");
