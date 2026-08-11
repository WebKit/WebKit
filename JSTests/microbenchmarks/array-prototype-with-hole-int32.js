const arr = [];
for (let i = 0; i < 1024; i++) {
    arr.push(i);
}
delete arr[512];

function test(index) {
    arr.with(index, 55);
}
noInline(test);

for (let i = 0; i < 1e4; i++) {
    test(i % 1024);
}
