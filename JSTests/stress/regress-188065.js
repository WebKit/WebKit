function test() {
    var arr = new Array(400);
    arr.concat([1.1]);
}
noInline(test);

for (var i = 0; i < 10000; i++)
    test();
