function test(strings)
{
    let result = 0;
    for (let i = 0; i < strings.length; ++i)
        result += parseInt(strings[i], 10);
    return result;
}
noInline(test);

// Shapes that miss the leading-'1'..'9' fast path: leading zero, sign, whitespace, 10+ digits.
let strings = ["0", "007", "-12345", "+6789", "  42", "1234567890", "0000000001", "-999999999"];
let expected = test(strings);

for (let i = 0; i < 1e6; ++i) {
    if (test(strings) !== expected)
        throw new Error("bad");
}
