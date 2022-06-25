function test(array) {
    let sum = 0;
    for (let v of array) {
        if (typeof v === "number")
            sum += v;
        else
            return sum;
    }
}
noInline(test);

let array = [1,2,3,4,"string"];
for (let i = 0; i < 1e5; ++i) {
    if (test(array) !== 10)
        throw new Error();
}


array = [1,2,3,4,{}]
for (let i = 0; i < 1e5; ++i) {
    if (test(array) !== 10)
        throw new Error();
}
