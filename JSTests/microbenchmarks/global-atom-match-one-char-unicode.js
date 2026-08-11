function test(str, regexp) {
    str.match(regexp);
}
noInline(test);

let str = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
let regexp = /a/gu;
for (let i = 0; i < 1e4; i++) {
    test(str, regexp);
}
