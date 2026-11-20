function test(str, regexp) {
    return str.match(regexp);
}
noInline(test);

let str = ("z".repeat(63) + "\n").repeat(160);
let regexp = /\n/g;
for (let i = 0; i < 1e4; i++) {
    let result = test(str, regexp);
    if (result.length !== 160)
        throw new Error("bad result: " + result.length);
}
