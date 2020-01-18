//@ requireOptions("--useClassFields=true")
//@ if isFTLEnabled then runFTLNoCJIT else skip end

let ftlTrue = $vm.ftlTrue;
let didFTLCompile = false;
function key(i) {
    return (i & 1) ? new String("foo") : "foo";
}
function stringOrStringObjectToString(i) {
    class C {
        [key(i)] = i;
    }
    didFTLCompile = ftlTrue();
    let c = new C();
    if (c.foo !== i)
        throw new Error(`Failed on iteration ${i}\n${JSON.stringify(c)}`);
}
noInline(stringOrStringObjectToString);

let i = 0;
let maxTries = 30000;
for (; i < maxTries && !numberOfDFGCompiles(stringOrStringObjectToString) && !didFTLCompile; ++i) {
    optimizeNextInvocation(stringOrStringObjectToString);
    stringOrStringObjectToString(i);
}

if (i >= maxTries)
    throw new Error("Failed to compile stringOrStringObjectToString with DFG JIT");
