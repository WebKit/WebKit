function test(x) {
    let pair = [,,];
    pair[0] = x + 0.5;
    pair[1] = x - 0.5;
    let quad = [,,,,];
    quad[0] = x;
    quad[3] = x;
    return pair[0] + pair[1] + quad.length;
}
noInline(test);

let result = 0;
for (let i = 0; i < 2e6; ++i)
    result += test(i);
if (result !== 3999998000000 + 8e6)
    throw new Error("bad result " + result);
