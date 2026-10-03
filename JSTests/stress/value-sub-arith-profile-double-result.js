function test(o) {
    let result = o.a - o.b;
    return result < 5;
}
noInline(test);
noOSRExitFuzzing(test);

for (let i = 0; i < testLoopCount * 10; ++i) {
    let a = (i & 7) ? i : undefined;
    if (test({ a, b: 3 }) !== (a - 3 < 5))
        throw new Error("bad result for " + a);
}
