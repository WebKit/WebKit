
function test(str) {
    return str.replace('h', "abc");
}
noInline(test);

let str = (-1).toLocaleString().padEnd(315241, "hello ");
for (let i = 0; i < 1e2; i++) {
    test(str);
}
