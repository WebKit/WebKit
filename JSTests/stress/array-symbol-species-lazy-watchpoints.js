// This tests that the lazy watchpoints we set for Symbol.species in our Builtin arrayPrototype functions work.


function test(array) {
    array = array.splice(2, 2);
    array = array.slice(0, 5);
    array = array.concat([1,2,3]);
    return array;
}
noInline(test);

function arrayEq(a, b) {
    if (a.length !== b.length)
        throw new Error();

    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            throw new Error();
    }
}

for (let i = 0; i < 100; i++)
    arrayEq(test([1,2,3,4,5,6,7,8,9]), [3,4,1,2,3]);

class A extends Array { }

for (let i = 0; i < 100; i++) {
    let result = test(new A(1,2,3,4,5,6,7,8,9));
    arrayEq(result, [3,4,1,2,3]);
    if (!(result instanceof A))
        throw new Error();
}

for (let i = 0; i < 100; i++)
    arrayEq(test([1,2,3,4,5,6,7,8,9]), [3,4,1,2,3]);

delete Array.prototype.sort;

for (let i = 0; i < 100; i++)
    arrayEq(test([1,2,3,4,5,6,7,8,9]), [3,4,1,2,3]);

for (let i = 0; i < 100; i++) {
    let result = test(new A(1,2,3,4,5,6,7,8,9));
    arrayEq(result, [3,4,1,2,3]);
    if (!(result instanceof A))
        throw new Error();
}
