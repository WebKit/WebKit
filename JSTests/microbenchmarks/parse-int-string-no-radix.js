function test(strings)
{
    let result = 0;
    for (let i = 0; i < strings.length; ++i)
        result += parseInt(strings[i]);
    return result;
}
noInline(test);

let strings = ["12345", "6789", "42", "100000", "7", "314159265", "0", "8888"];
let expected = test(strings);

for (let i = 0; i < 1e6; ++i) {
    if (test(strings) !== expected)
        throw new Error("bad");
}
