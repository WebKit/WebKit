const arr = [];
for (let i = 0; i < 1024; i++) {
    arr.push(i + 0.1);
}
delete arr[512];

function test(index) {
    arr.with(index, 5.5);
}
noInline(test);

for (let i = 0; i < 1e4; i++) {
    test(i % 1024);
}
