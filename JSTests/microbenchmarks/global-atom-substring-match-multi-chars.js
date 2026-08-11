
function test(str, regexp) {
    str.substring(0, 50).match(regexp);
    str.substring(0, 60).match(regexp);
}
noInline(test);

let str = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
let regexp = /aaaaaaaaaa/g;
for (let i = 0; i < 1e4; i++) {
    test(str, regexp);
}
