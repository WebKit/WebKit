// op_iterator_next with wide16 and wide32 operands, over an Array that is iterated without an iterator object.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

function makeWithLocals(count) {
    let names = [];
    for (let i = 0; i < count; i++)
        names.push("v" + i);
    return eval(`(function (array) {
        let ${names.join(",")};
        let seen = [];
        for (let x of array)
            seen.push(x);
        let [a, b = "default", ...rest] = array;
        return seen.join() + "|" + a + "," + b + "," + rest.length;
    })`);
}

function makeArrayStorage() {
    let array = [1, 2, 3];
    array[100000] = 4;
    array.length = 3;
    return array;
}

const cases = [
    ["int32", [1, 2, 3], "1,2,3|1,2,1"],
    ["contiguous", ["a", "b", "c"], "a,b,c|a,b,1"],
    ["double", [1.5, 2.5, 3.5], "1.5,2.5,3.5|1.5,2.5,1"],
    ["empty", [], "|undefined,default,0"],
    ["hole", [1, , 3], "1,,3|1,default,1"],
    ["array storage", makeArrayStorage(), "1,2,3|1,2,1"],
];

let wide16 = makeWithLocals(200);
let wide32 = makeWithLocals(33000);
noInline(wide16);
noInline(wide32);

for (let i = 0; i < testLoopCount; i++) {
    for (let [name, array, expected] of cases)
        shouldBe(wide16(array), expected, "wide16 " + name);
}
for (let i = 0; i < 10; i++) {
    for (let [name, array, expected] of cases)
        shouldBe(wide32(array), expected, "wide32 " + name);
}
