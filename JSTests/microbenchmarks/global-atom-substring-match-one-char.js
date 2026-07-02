
function test(str, regexp) {
    str.substring(0, 50).match(regexp);
    str.substring(0, 60).match(regexp);
}
noInline(test);

let str = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";
let regexp = /\n/g;
for (let i = 0; i < 1e4; i++) {
    test(str, regexp);
}

