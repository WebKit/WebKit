//@ runFTLNoCJIT

Array.prototype.__defineGetter__(100, () => 1);

let childGlobal = createGlobalObject();
let a = new childGlobal.Array(2.3023e-320, 2.3023e-320, 2.3023e-320, 2.3023e-320, 2.3023e-320, 2.3023e-320);

var tierWarmUpIterations = [
    1, // LLInt
    50, // baseline JIT
    500, // DFG
    10000, // FTL
];

function doTest(warmUpIterations) {
    var test = new Function("a", "return Array.prototype.slice.call(a).toString();");
    noInline(test);

    for (var i = 0; i < warmUpIterations; i++)
        test([1, 2, 3]);

    test(a);
}

for (var warmUpIterations of tierWarmUpIterations)
    doTest(warmUpIterations);

