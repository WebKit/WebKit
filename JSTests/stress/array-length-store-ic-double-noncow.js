function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("got " + actual + ", expected " + expected);
}

function f(a, n) { a.length = n; }
noInline(f);

for (let i = 0; i < testLoopCount; ++i) {
    let a = [1.5];
    a.push(2.5);
    a.push(3.5);
    a.push(4.5);
    f(a, 1);
    shouldBe(a.length, 1);
    shouldBe(a[0], 1.5);
    shouldBe(a[1], undefined);
    shouldBe(a[3], undefined);
    shouldBe(1 in a, false);
    a.push(9.5);
    shouldBe(a.length, 2);
    shouldBe(a[1], 9.5);
}
