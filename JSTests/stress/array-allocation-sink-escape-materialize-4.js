function conditionalGetButterfly(shouldEscape) {
    let a = new Array(5);
    a[0] = 42;
    a[1] = 43;

    let len = a.length;

    if (shouldEscape) {
        return len;
    }

    return a[0] + a[1];
}
noInline(conditionalGetButterfly);

for (let i = 0; i < testLoopCount; i++) {
    conditionalGetButterfly(i % 10 === 0);
}