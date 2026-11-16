
function test(x) {
    const a = new Array(4);
    let v = 50;
    for (let i = 0; i < 100; v++) {
        a[i] = Math.random();
        if (v % 9 == 0)
            return;
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    test(i);
}
