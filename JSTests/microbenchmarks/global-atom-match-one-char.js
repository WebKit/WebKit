
function test(str, regexp) {
    str.match(regexp);
}
noInline(test);

let str = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
let regexp = /a/g;
for (let i = 0; i < 1e4; i++) {
    test(str, regexp);
}

