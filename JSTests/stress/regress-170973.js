// This test passes if it does not crash.

function test(i0, i1) {
    i0 = i0|0;
    i1 = i1|0;
    if (i1 & 1)
        i1 = (((i0 ? i1 : i1)-i0) ? false : 0);
}
noInline(test);

for (var k = 0; k < 200; ++k) {
    if (k < 100)
        test(0, 0x80000001);
    else
        test(0x800, 0x80000001);
}
