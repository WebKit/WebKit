function test() {
    let obj = {};
    [1,2,3].concat(4);
    [1,2,3].concat(1.34);
    [1.35,2,3].concat(1.34);
    [1.35,2,3].concat(obj);
    [1,2,3].concat(obj);
    [].concat(obj);
    [].concat(1);
    [].concat(1.224);
}
noInline(test);

for (let i = 0; i < 10000; i++)
    test();
