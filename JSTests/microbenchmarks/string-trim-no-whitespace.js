function test(string) {
    return string.trim().length + string.trimStart().length + string.trimEnd().length;
}
noInline(test);

const strings = [];
for (let i = 0; i < 16; ++i)
    strings.push('field-' + i + '-value-' + (i * 7));

let result = 0;
for (let i = 0; i < 2e6; ++i)
    result += test(strings[i & 15]);

if (result < 0)
    throw new Error('bad result: ' + result);
