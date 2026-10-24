function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const pieces = ['A', 'AC', 'AAC', 'AACA', ',', 'CAA', 'E', 'AAAA'];

const array = [];
for (let i = 0; i < 8; ++i) {
    let string = '';
    for (let j = 0; j < 256; ++j)
        string += pieces[(i + j) & 7];
    array.push(string);
}

for (var i = 0; i < 2e4; ++i)
    shouldBe(array.join('').length, 4864);
