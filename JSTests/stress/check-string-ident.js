//@ defaultNoEagerRun

const o = { baz: 20 };
function foo(p) {
    o[p] = 20;
}
noInline(foo);
noOSRExitFuzzing(foo);

for (let i = 0; i < 1000000; i++) {
    foo("baz");
}

if (numberOfDFGCompiles(foo) > 1)
    throw new Error("We should not have to compile this function more than once.");
