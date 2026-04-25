function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error((msg || "") + " expected " + expected + " but got " + actual);
}

{
    let ta = new Int32Array(100);
    for (let i = 0; i < 100; i++) ta[i] = 100 - i;

    ta.sort((a, b) => {
        ta.buffer;
        return a - b;
    });

    shouldBe(ta[0], 1, "ta[0] after sort with .buffer access");
    shouldBe(ta[99], 100, "ta[99] after sort with .buffer access");
    for (let i = 0; i < 99; i++)
        shouldBe(ta[i] <= ta[i+1], true, "sorted at [" + i + "]");
}

{
    let ta = new Float64Array(50);
    for (let i = 0; i < 50; i++) ta[i] = Math.random();

    let accessed = false;
    ta.sort((a, b) => {
        if (!accessed) { ta.buffer; accessed = true; }
        return a - b;
    });

    for (let i = 0; i < 49; i++)
        shouldBe(ta[i] <= ta[i+1], true, "Float64 sorted at [" + i + "]");
}

{
    let ta = new Int32Array(100);
    ta.buffer;
    for (let i = 0; i < 100; i++) ta[i] = 100 - i;

    ta.sort((a, b) => { ta.buffer; return a - b; });

    shouldBe(ta[0], 1, "pre-wasteful ta[0]");
    shouldBe(ta[99], 100, "pre-wasteful ta[99]");
}
