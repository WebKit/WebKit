//@ $skipModes << :lockdown if $buildType == "debug"

function max(array) {
    return Math.max(...array);
}
noInline(max);

const arrays = [];
for (let i = 0; i < 8; ++i) {
    const array = [];
    for (let j = 0; j < 5 + i * 2; ++j)
        array.push(i & 1 ? j + 0.5 : j);
    arrays.push(array);
}

let result = 0;
for (let i = 0; i < 2e6; ++i)
    result += max(arrays[i & 7]);
if (result !== 2e6 / 8 * (4 + 6.5 + 8 + 10.5 + 12 + 14.5 + 16 + 18.5))
    throw new Error(result);
