//@ runDefault("--useConcurrentGC=false", "--useConcurrentJIT=false", "--thresholdForFTLOptimizeAfterWarmUp=1000")

var functions = [];

function boo() {
    functions.push(new Function("a", "return a"));
    return functions.splice(0);
}

function test() {
    functions = boo().slice();
}

noDFG(boo);
noInline(boo);
noInline(test);

for (var i = 0; i < 10000; i++)
    test();
