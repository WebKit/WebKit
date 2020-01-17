
let currentArgCount;
function expectedArgCount() {
    return currentArgCount;
}
noInline(expectedArgCount);

function callee() {
    if (arguments.length != expectedArgCount())
        throw new Error();
}

function test(array) {
    callee.apply(undefined, array);
}
noInline(test);

let lengthCalls = 0;
currentArgCount = 2;
let array = { 0: 1, 1: 2, get length() {
    if (lengthCalls++ % 10 == 1)
        throw new Error("throwing an exception in length");
    return currentArgCount
} }
for (let i = 0; i < 1e6; i++) {
    try {
        test(array);
    } catch { }
}

currentArgCount = 100;
lengthCalls = 0;
test(array);

if (lengthCalls !== 1)
    throw new Error(lengthCalls);
