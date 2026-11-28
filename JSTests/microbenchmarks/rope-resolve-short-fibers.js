function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const pieces = ['A', 'AC', 'AAC', 'AACA', ',', 'CAA', 'E', 'AAAA'];

for (var i = 0; i < 100; ++i) {
    let string = '';
    for (let i = 0; i < 3e4; ++i)
        string += pieces[i & 7];
    shouldBe(string.length, 71250);
    shouldBe(string[30], 'C');
}
