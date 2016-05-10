function testInLoopTests(array, index)
{
    let arrayLength = array.length;
    let sum = 0;
    for (let i = 0; i < 10; ++i) {
        if (index >= 0 && index < arrayLength) {
            sum += array[index];
        }
    }
    return sum;
}
noInline(testInLoopTests);


let testArray = [1, 2, 3];

// Warmup "in-bounds" up to FTL.
for (let i = 0; i < 1e5; ++i) {
    if (testInLoopTests(testArray, 1) !== 20)
        throw "Failed testInLoopTests(testArray, 1)"
    if (testInLoopTests(testArray, 2) !== 30)
        throw "Failed testInLoopTests(testArray, 2)"
}

let largeIntResult = testInLoopTests(testArray, 2147483647);
if (largeIntResult !== 0)
    throw "Failed testInLoopTests(testArray, 2147483647)";
let smallIntResult = testInLoopTests(testArray, -2147483647);
if (smallIntResult !== 0)
    throw "Failed testInLoopTests(testArray, -2147483647)";
