// While this is not required by the spec. It looks like some websites rely on the comparator being null.

function assertEq(a, b) {
    if (a !== b)
        throw new Error();
}

let array = [2,1].sort(null);
assertEq(array[0], 1);
assertEq(array[1], 2);
