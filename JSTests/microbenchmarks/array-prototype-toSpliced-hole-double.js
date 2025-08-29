const arr = [];
for (let i = 0; i < 1024; i++) {
    arr.push(i + 0.1);
}
delete arr[512];

function test(deleteCount) {
    arr.toSpliced(400, deleteCount, 9999.9, 8888.8);
}
noInline(test);

for (let i = 0; i < 1e4; i++) {
    test(i % 100);
}
