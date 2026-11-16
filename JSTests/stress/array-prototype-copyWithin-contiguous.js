function compareArray(actual, expected) {
    if (actual.length !== expected.length)
        throw new Error(`Expected length ${expected.length} but got ${actual.length}`);
    for (let i = 0, l = actual.length; i < l; ++i) {
        if (Object.hasOwn(actual, i) !== Object.hasOwn(expected, i))
            throw new Error(`${i}: Mismatch the owner of the property`);
        if (actual[i] !== expected[i])
            throw new Error(`${i}: Expected ${expected[i]} but got ${actual[i]}`);
    }
}

{
    let obj1 = { a: 1 };
    let obj2 = { b: 2 };
    let arr = [obj1, "hello", 123, obj2, null, "end"];
    arr.copyWithin(1, 3, 5); 
    compareArray(arr, [obj1, obj2, null, obj2, null, "end"]);
}

{
    let obj1 = {};
    let obj2 = {};
    let arr = ["x", "y", "z", obj1, obj2, "tail"];
    arr.copyWithin(2, 1); 
    compareArray(arr, ["x", "y", "y", "z", obj1, obj2]);
}

{
    let obj = { test: 123 };
    let arr = [null, obj, "aaa", 42];
    arr.copyWithin(1, 2, 2); 
    compareArray(arr, [null, obj, "aaa", 42]);
}

{
    // Have a hole on array by literal.
    const obj = { test: 123 };
    const arr = [null, , obj, , "aaa", , 42];
    arr.copyWithin(2, 3);
    compareArray(arr, [null, , , "aaa", , 42, 42]);
}

{
    // Have a hole by array constructor.
    const obj = { test: 123 };
    const arr = new Array(7);
    arr[0] = null;
    arr[2] = obj;
    arr[4] = "aaa";
    arr[6] = 42;
    arr.copyWithin(2, 3);
    compareArray(arr, [null, , , "aaa", , 42, 42]);
}

{
    // Have a hole by chainging Array#length
    const arr = new Array(0);
    arr.length = 5;
    arr[2] = 2;
    arr.copyWithin(2, 0);
    compareArray(arr, [, , , , 2]);
}

{
    // Have a hole by delete indexed prop
    const obj = { test: 123 };
    const arr = [null, obj, "aaa", 42];
    delete arr[1];
    arr.copyWithin(0, 1, 2);
    compareArray(arr, [, , "aaa", 42]);
}