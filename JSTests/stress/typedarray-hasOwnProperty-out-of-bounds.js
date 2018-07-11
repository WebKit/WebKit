
let array = new Float32Array(10);

function test(array, indicies, result) {
    for (let i of indicies) {
        if (array.hasOwnProperty(i) !== result)
            throw new Error("wrong value for " + i);
        if (array.hasOwnProperty(i.toString()) !== result)
            throw new Error("wrong value for " + i + " (as String)");
    }
}
noInline(test);

let interestingIndicies = [0, 1, 2, 8, 9];
for (let i = 0; i < 10000; i++)
    test(array, interestingIndicies, true);

interestingIndicies = [-1, 10, 100];
for (let i = 0; i < 10000; i++)
    test(array, interestingIndicies, false);
