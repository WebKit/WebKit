function test(obj) {
    let array = new Array(4);
    array[0] = 42;
    array[1] = array;
    for (let i = 2; i < array.length; ++i)
        array[i] = array[0];

    if (obj.foo === 1)
        return 0;
    let property = array[1];
    if (property !== array)
        throw new Error(property);
    return array[0] + array[2] - array[3];
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i) {
    let result = test({foo: i % 2});
    if (result !== 42 && result !== 0)
        throw new Error(result);
}

let result = test({foo: "hello"});
if (result !== 42)
    throw new Error(result);
