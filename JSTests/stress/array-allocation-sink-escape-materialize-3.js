
function test(b) {
    let a = new Array(4);
    if (b % 2 == 0) {
        return a.length;
    } else {
        if (b % 13 == 0) {
            a[0] = 15 + a.length;
        } else {
            a[1] = 15 + a.length;
        }
        a[2] += a.length;
        return a;
    }
}
noInline(test);

for (let i = 0; i < testLoopCount; i++) {
    test(i);
}
