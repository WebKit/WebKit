function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function compare(a, b) {
    OSRExit();
    return a - b;
}

function compareWithBoundArguments(sign, a, b) {
    OSRExit();
    return this.factor * sign * (a - b);
}

function compareWithTailCall(a, b) {
    "use strict";
    return compare(a, b);
}

const bound = compare.bind(null);
const boundTwice = bound.bind(null);
const boundWithArguments = compareWithBoundArguments.bind({ factor: 2 }, -1);
const boundWithTailCall = compareWithTailCall.bind(null);

function useResult(a, b, c) {
    let array = [a, b, c];
    let result = array.sort(bound);
    shouldBe(result, array);
    return result;
}

function ignoreResult(a, b, c) {
    let array = [a, b, c];
    array.sort(bound);
    return array;
}

function tailCall(a, b, c) {
    "use strict";
    return [a, b, c].sort(bound);
}

function callTailCall(a, b, c) {
    return [tailCall(a, b, c)];
}

function sortBoundTwice(a, b, c) {
    return [a, b, c].sort(boundTwice);
}

function sortBoundWithArguments(a, b, c) {
    return [a, b, c].sort(boundWithArguments);
}

function sortBoundWithTailCall(a, b, c) {
    return [a, b, c].sort(boundWithTailCall);
}

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(useResult(3, 1, 2).join(), "1,2,3");
    shouldBe(ignoreResult(3, 1, 2).join(), "1,2,3");
    shouldBe(tailCall(3, 1, 2).join(), "1,2,3");
    shouldBe(callTailCall(3, 1, 2)[0].join(), "1,2,3");
    shouldBe(sortBoundTwice(3, 1, 2).join(), "1,2,3");
    shouldBe(sortBoundWithArguments(3, 1, 2).join(), "3,2,1");
    shouldBe(sortBoundWithTailCall(3, 1, 2).join(), "1,2,3");
}
