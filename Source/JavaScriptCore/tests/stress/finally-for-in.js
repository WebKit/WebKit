function repeat(count, thunk) {
    let result = "";
    for (let i = 0; i < count; i++)
        result += thunk(i);
    return result;
}

function createSimple(outerDepth, innerDepth, returnDepth) {
    return Function(
        `
        return (function(arg) {
            ${repeat(outerDepth, (i) => `for (let a${i} in arg) ` + "{\n" )}
            try {
                ${repeat(innerDepth, (i) => `for (let b${i} in arg) ` + "{\n" )}
                return {};
                ${repeat(innerDepth, () => "}")}
            }
            finally { return a${returnDepth}}
            ${repeat(outerDepth, () => "}")}
        })
        `
    )();
}

function test(result, argument, ...args) {
    let f = createSimple(...args);

    let r = f(argument);
    if (r !== result) {
        throw new Error(r);
    }
}


test("0", [1,2], 1, 1, 0);
test("0", [1,2], 2, 1, 0);
test("0", [1,2], 2, 4, 1);
test("0", [1,2], 1, 0, 0);
