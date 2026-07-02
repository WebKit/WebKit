function compareArray(a, b) {
    if (a.length !== b.length)
        throw new Error(`Expected length ${b.length} but got ${a.length}`);
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            throw new Error(`${i}: Expected ${b[i]} but got ${a[i]}`);
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
