function test(obj) {
    let array = new Array(4);
    array[0] = {};
    if (obj.foo === 1)
        return 0;
    return array[0];
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    test({foo: i % 2});

test({foo: "hello"});