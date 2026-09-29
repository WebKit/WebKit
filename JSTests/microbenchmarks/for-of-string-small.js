const string = "abcdefghij";

function test(string) {
    let count = 0;
    for (let character of string)
        count += character.length;
    return count;
}
noInline(test);

for (let i = 0; i < 1e5; ++i)
    test(string);
