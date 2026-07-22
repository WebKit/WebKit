//@ runDefault("--thresholdForJITAfterWarmUp=10", "--thresholdForFTLOptimizeAfterWarmUp=1000", "--useConcurrentJIT=false")

"use strict";

function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected);
}
noInline(shouldBe);

function five(values1, values2)
{
    let result = null;
    for (let i = 0; i < 5; ++i) {
        function arg() { "use strict"; return arguments; }
        const a = arg.apply(undefined, values1);
        const b = arg.apply(undefined, values2);
        try {
            (3881)(b);
        } catch (error) {
            a.toString();
            result = a;
        }
    }
    return result;
}
noInline(five);

function eight(values1, values2)
{
    let result = null;
    for (let i = 0; i < 5; ++i) {
        function arg() { "use strict"; return arguments; }
        const a = arg.apply(undefined, values1);
        const b = arg.apply(undefined, values2);
        try {
            (3881)(b);
        } catch (error) {
            a.toString();
            result = a;
        }
    }
    return result;
}
noInline(eight);

function filled(length, value)
{
    const result = [];
    for (let i = 0; i < length; ++i)
        result.push(value);
    return result;
}
noInline(filled);

const fiveMarker = { marker: "five" };
const eightMarker = { marker: "eight" };
const seedArray = [{ marker: "seed" }, 1, 2, 3, 4, 5];

const firstFive = filled(5, fiveMarker);
const overwriteFive = filled(30, fiveMarker);
overwriteFive[22] = 9;

const firstEight = filled(8, eightMarker);
const overwriteEight = filled(30, eightMarker);
overwriteEight[20] = 9;

for (let i = 0; i < testLoopCount; ++i) {
    five(firstFive, overwriteFive);
    eight(firstEight, overwriteEight);
}

const seedValues = filled(30, seedArray);
seedValues[20] = 9;
for (let i = 0; i < testLoopCount; ++i)
    eight(firstEight, seedValues);

const recoveredEight = eight(firstEight, seedValues);
shouldBe(recoveredEight.length, firstEight.length);
for (let i = 0; i < firstEight.length; ++i)
    shouldBe(recoveredEight[i], eightMarker);

const recoveredFive = five(firstFive, overwriteFive);
shouldBe(recoveredFive.length, firstFive.length);
for (let i = 0; i < firstFive.length; ++i)
    shouldBe(recoveredFive[i], fiveMarker);
shouldBe(recoveredFive[5], undefined);
