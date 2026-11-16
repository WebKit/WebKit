const string = "abcdefghij";

function test(string, wanted) {
    let index = 0;
    for (let character of string) {
        if (character === wanted)
            break;
        index++;
    }
    return index;
}
noInline(test);

for (let i = 0; i < 1e5; ++i)
    test(string, "e");
