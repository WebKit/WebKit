// Verify that DFG TryGetById nodes properly save live registers.  This test should not crash.

function tryMultipleGetByIds() { return '(function (base) { return @tryGetById(base, "value1") + @tryGetById(base, "value2") + @tryGetById(base, "value3"); })'; } 


let get = createBuiltin(tryMultipleGetByIds());
noInline(get);

function test() {
    let obj1 = {
        value1: "Testing, ",
        value2: "testing, ",
        value3: "123",
        expected: "Testing, testing, 123"
    };
    let obj2 = {
        extraFieldToMakeThisObjectDifferentThanObj1: 42,
        value1: 20,
        value2: 10,
        value3: 12,
        expected: 42
    };

    let objects = [obj1, obj2];

    for (let i = 0; i < 200000; i++) {
        let obj = objects[i % 2];
        if (get(obj) !== obj.expected)
            throw new Error("wrong on iteration: " + i);
    }
}

test();
