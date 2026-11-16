function test(x) {
    return typeof x > "u";
}
noInline(test);

for (let i = 0; i < 1e6; i++) {
    test(i % 2 === 0 ? undefined : i);
}
