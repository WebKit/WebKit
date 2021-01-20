let count = 0;

function foo(array) {
    for (let v of array)
        throw new Error();
}
noInline(foo);

let otherGlobal = createGlobalObject();
let array = otherGlobal.Array();
array[Symbol.iterator]().__proto__.next = function () {
    count++;
    return { done: true };
}
array[0] = 1;

const iterCount = 1e5;
for (let i = 0; i < iterCount; ++i)
    foo(array);

if (iterCount !== count)
    throw new Error();
