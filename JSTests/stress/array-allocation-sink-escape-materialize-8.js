function controlFlowTest(cond1, cond2) {
    let a, b;

    if (cond1) {
        a = new Array(3);
        a[0] = 100;
        a[1] = 200;
    } else {
        a = new Array(4);
        a[0] = 300;
        a[1] = 400;
        a[2] = 500;
    }

    let len = a.length;

    if (cond2) {
        return a;
    } else {
        return len + a[0];
    }
}
noInline(controlFlowTest);

for (let i = 0; i < testLoopCount; i++) {
    controlFlowTest(i % 2 === 0, i % 3 === 0);
}