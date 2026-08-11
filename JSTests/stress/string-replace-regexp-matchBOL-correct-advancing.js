// Check that we don't advance for BOL assertions when matching a non-BMP character in the YARR interpreter
// and that we do advance in String.replace() when processing an empty match.

let expected = "|";

for (let i = 0; i < 11; ++i)
    expected += String.fromCodePoint(128512) + '|';

let str = String.fromCodePoint(128512).repeat(11);

let result1 = str.replace(/(?!(?=^a|()+()+x)(abc))/gmu, r => {
    return '|';
});


if (result1 !== expected)
    print("FAILED: \"" + result1 + " !== " + expected + '"');

let result2= str.replace(/(?!(?=^a|x)(abc))/gmu, r => {
    return '|';
});

if (result2 !== expected)
    print("FAILED: \"" + result2 + " !== " + expected + '"');

expected = "|" + String.fromCodePoint(128512);

str = String.fromCodePoint(128512).repeat(1);

let result3= str.replace(/(?!(?=^a|x)(abc))/mu, r => {
    return '|';
});

if (result3 !== expected)
    print("FAILED: \"" + result3 + " !== " + expected + '"');
