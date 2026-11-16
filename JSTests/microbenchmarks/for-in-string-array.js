function test(object) {
    sum = "";
    for (var i in object)
        sum += object[i];
    return sum;
}
noInline(test);

for (let i = 0; i < 1e5; ++i)
    test(Array(100).fill("a"));
