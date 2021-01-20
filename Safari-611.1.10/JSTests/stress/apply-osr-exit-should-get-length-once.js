
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
let array = { 0: 1, 1: 2, get length() { lengthCalls++; return currentArgCount } }
for (let i = 0; i < 1e5; i++)
    test(array);


test(array);
currentArgCount = 100;
lengthCalls = 0;
test(array);

if (lengthCalls !== 1)
    throw new Error(lengthCalls);
