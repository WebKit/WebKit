//@ $skipModes << :lockdown

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)} expected: ${String(expected)}`);
}

// The spread of the rest is eliminated while the spread of the other array is not, so the arguments of the
// frame which the rest is created from are read from the stack.
function sloppyRest(x, ...rest) { return join(...rest, ...globalArray, x); }
function callSloppyRest(a, b) { return sloppyRest(a + 1, b + 2, a + b); }
noInline(callSloppyRest);

const strict = function () {
    "use strict";

    function join(a, b, c, d, e) { return `${arguments.length}:${a},${b},${c},${d},${e}`; }
    function joinLast() { return `${arguments.length}:${arguments[arguments.length - 1]}`; }

    function rest(x, ...rest) { return join(...rest, ...globalArray, x); }
    function callRest(a, b) { return rest(a + 1, b + 2, a + b); }
    noInline(callRest);

    function restAfterMaterialized(x, ...rest) { return join(...globalArray, x, ...rest); }
    function callRestAfterMaterialized(a, b) { return restAfterMaterialized(a + 1, b + 2, a + b); }
    noInline(callRestAfterMaterialized);

    function restProven(array, ...rest) { return array.length + join(...array, ...rest); }
    function callRestProven(array, a, b) { return restProven(array, a + 1, b + 2, a + b); }
    noInline(callRestProven);

    function restTwice(x, ...rest) { return joinLast(...rest, ...globalArray, ...rest) + join(...rest, ...globalArray, x); }
    function callRestTwice(a, b) { return restTwice(a + 1, b + 2, a + b); }
    noInline(callRestTwice);

    function callRestVarargs(array, x) { return rest(x + 1, ...array); }
    noInline(callRestVarargs);

    function callRestUnderBranch(flag, a, b) { return restUnderBranch(flag, a + 1, b + 2); }
    function restUnderBranch(flag, ...rest) {
        if (flag)
            return join(...rest, ...globalArray);
        return join(0, ...globalArray);
    }
    noInline(callRestUnderBranch);

    function machineFrame(x, ...rest) { return join(...rest, ...globalArray, x); }
    noInline(machineFrame);

    function constantArray(a) { return join(...[1.5, 2.5], ...globalArray, a); }
    noInline(constantArray);

    return { callRest, callRestAfterMaterialized, callRestProven, callRestTwice, callRestVarargs, callRestUnderBranch, machineFrame, constantArray };
}();

function join(a, b, c, d, e) { return `${arguments.length}:${a},${b},${c},${d},${e}`; }

var globalArray = [10, 20];
const int32s = [2, 3];

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(callSloppyRest(i, 2), `5:4,${i + 2},10,20,${i + 1}`);
    shouldBe(strict.callRest(i, 2), `5:4,${i + 2},10,20,${i + 1}`);
    shouldBe(strict.callRestAfterMaterialized(i, 2), `5:10,20,${i + 1},4,${i + 2}`);
    shouldBe(strict.callRestProven(int32s, i, 2), `25:2,3,${i + 1},4,${i + 2}`);
    shouldBe(strict.callRestTwice(i, 2), `6:${i + 2}5:4,${i + 2},10,20,${i + 1}`);
    shouldBe(strict.callRestVarargs(int32s, i), `5:2,3,10,20,${i + 1}`);
    shouldBe(strict.callRestVarargs([], i), `3:10,20,${i + 1},undefined,undefined`);
    shouldBe(strict.callRestUnderBranch(i & 1, i, 2), i & 1 ? `4:${i + 1},4,10,20,undefined` : "3:0,10,20,undefined,undefined");
    shouldBe(strict.machineFrame(i, 1, 2), `5:1,2,10,20,${i}`);
    shouldBe(strict.machineFrame(i), `3:10,20,${i},undefined,undefined`);
    shouldBe(strict.constantArray(i), `5:1.5,2.5,10,20,${i}`);
}

const large = new Array(1000).fill(1);
for (let i = 0; i < 5; ++i) {
    globalArray = large;
    shouldBe(strict.callRest(i, 2), `1003:4,${i + 2},1,1,1`);
    shouldBe(strict.callRestAfterMaterialized(i, 2), `1003:1,1,1,1,1`);
    shouldBe(strict.callRestTwice(i, 2), `1004:${i + 2}1003:4,${i + 2},1,1,1`);
}
