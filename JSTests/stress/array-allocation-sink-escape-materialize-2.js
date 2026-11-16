
function test(b) {
    let a = new Array(4);
    a[0] = 43;
    a.length;
}
noInline(test);

for (let i = 0; i < testLoopCount; i++) {
    test(i);
}
