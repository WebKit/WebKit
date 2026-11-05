function backward(array) {
    let result = 0;
    for (let i = array.length - 3; i >= 0; --i)
        result += array[i];
    return result;
}
noInline(backward);

let array = [];
for (let i = 0; i < 1000; ++i)
    array.push(i);

for (let i = 0; i < 2e5; ++i) {
    if (backward(array) !== 497503)
        throw new Error("bad result");
}
