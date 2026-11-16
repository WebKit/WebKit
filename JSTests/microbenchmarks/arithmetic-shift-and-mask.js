function test(pixel)
{
    var r = (pixel >> 24) & 0xff;
    var g = (pixel >> 16) & 0xff;
    var b = (pixel >> 8) & 0xff;
    var a = pixel & 0xff;
    return r + g + b + a;
}
noInline(test);

var result = 0;
for (var i = 0; i < 5e6; ++i) {
    result += test(0x12345678);
    result += test(0x87654321 | 0);
}

if (result !== 3060000000)
    throw new Error("bad result: " + result);
