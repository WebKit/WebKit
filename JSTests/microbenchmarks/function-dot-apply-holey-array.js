function count(a) {
    return arguments.length + (a === undefined ? 1 : 0);
}
noInline(count);

function test(array) {
    return count.apply(null, array);
}
noInline(test);

const array = [0, 1, 2, 3, 4, 5, 6, 7];
delete array[0];

let result = 0;
for (let i = 0; i < 1e6; ++i)
    result += test(array);
if (result !== 9e6)
    throw new Error("bad result: " + result);
