function test() {
    let a = [];
    for (let i = 0; i < 1e6; ++i) {
        a.push(1);
        a.push(2);
        a.push(3);
        a.length = 0;
    }
    return a.length;
}
noInline(test);
for (let i = 0; i < 5; ++i)
    test();
