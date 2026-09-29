const strings = ["ab", "cd", "ef", "gh"];

function test(string) {
    let [first, second] = string;
    return first.length + second.length;
}
noInline(test);

for (let i = 0; i < 1e6; ++i)
    test(strings[i & 3]);
