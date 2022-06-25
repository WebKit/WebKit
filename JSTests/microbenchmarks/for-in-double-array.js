function test(object) {
    sum = 0;
    for (var i in object)
        sum += object[i];
    return sum;
}
noInline(test);

for (let i = 0; i < 1e5; ++i)
    test(Array(100).fill(1.1231241));
