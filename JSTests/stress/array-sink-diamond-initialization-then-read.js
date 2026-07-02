function test1(obj, flag) {
    let array = new Array(4);
    if (flag)
        array[0] = 1;
    else
        array[0] = obj.foo;
    return array[0];
}
noInline(test1);


let obj1 = { foo: 1 };
for (let i = 0; i < testLoopCount; ++i) {
    test1(obj1, i % 2);
}