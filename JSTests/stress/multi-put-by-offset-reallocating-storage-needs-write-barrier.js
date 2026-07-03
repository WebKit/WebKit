//@ runDefault("--useConcurrentJIT=false", "--jitPolicyScale=0.1", "--useConcurrentGC=false", "--verifyGC=true", "--gcMaxHeapSize=500000")

var g_value = {};

function makeObj() { return {}; }

function foo(cond) {
    let a = makeObj();
    a.p1 = 1;
    a.p2 = 1;
    a.p3 = 1;
    a.p4 = 1;
    a.p5 = 1;
    if (cond)
        a.y = 1;
    else
        a.z = 1;
    a.x = g_value;
    return a;
}
noInline(foo);

for (let i = 0; i < testLoopCount; ++i) {
    g_value = {};
    foo(i & 1);
}

var holder = new Array(100000).fill(null);
fullGC();

for (let i = 0; i < testLoopCount * 10; ++i)
    holder[i % 100000] = foo(i & 1);
