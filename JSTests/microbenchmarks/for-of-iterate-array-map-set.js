function sum(obj) {
    let result = 0;
    for (let x of obj) {
        if (typeof x == "number")
            result += x;
        else
            result += x[0];
    }
    return result;
}
noInline(sum);

let array = [1,2,3,4,5];
let set = new Set(array);
let map = new Map(array.map(x => [x, x]));
let generator = function* (array) {
    for (let i = 0; i < array.length; ++i)
        yield array[i];
}

for (let i = 0; i < 1e5; ++i) {
    if (sum(set) !== 15)
        throw new Error();
    if (sum(generator(array)) !== 15)
        throw new Error();
}
