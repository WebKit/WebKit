const arr = [];
for (let i = 0; i < 1024; i++) {
    arr.push(i + 0.5);
}

function test() {
    arr.toReversed();
}
noInline(test);

for (let i = 0; i < 1e4; i++) {
    test();
}
