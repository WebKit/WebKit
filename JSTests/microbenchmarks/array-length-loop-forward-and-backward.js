function forward(array) {
    let result = 0;
    for (let i = 0; i < array.length; ++i)
        result += array[i];
    return result;
}
noInline(forward);

function backward(array) {
    let result = 0;
    for (let i = array.length - 1; i >= 0; --i)
        result += array[i];
    return result;
}
noInline(backward);

let array = [];
for (let i = 0; i < 1000; ++i)
    array.push(i);

for (let i = 0; i < 2e5; ++i) {
    if (forward(array) !== 499500)
        throw new Error("bad result");
    if (backward(array) !== 499500)
        throw new Error("bad result");
}
