function test(array) {
    let result = 0;
    for (let i = 0; i < 1e4; ++i) {
        if (Object.isFrozen(array))
            ++result;
        if (Object.isSealed(array))
            ++result;
    }
    return result;
}
noInline(test);

let array = [];
for (let i = 0; i < 32; ++i)
    array.push(i);
let result = test(Object.freeze(array));
if (result !== 2e4)
    throw new Error("bad result: " + result);
