function sum() {
    var result = 0;
    for (var value of arguments)
        result += value;
    return result;
}
noInline(sum);

for (var i = 0; i < 1e6; ++i) {
    if (sum(i, 1, 2, 3, 4, 5, 6, 7) !== i + 28)
        throw new Error("bad sum");
}
