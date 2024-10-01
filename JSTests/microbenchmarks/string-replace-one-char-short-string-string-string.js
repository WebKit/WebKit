
function test(str) {
    return str.replace('h', "abc");
}
noInline(test);

let str = (-1).toLocaleString().padEnd(296, "hello ");
for (let i = 0; i < 1e4; i++) {
    test(str);
}
