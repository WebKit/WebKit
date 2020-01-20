//@ skip if $model == "Apple Watch Series 3" or (not $jitTests and $architecture =~ /arm|mips/ and $hostOS == "linux") # added by mark-jsc-stress-test.py
let glob = 0

function doTest(number, bit) {
    glob -= 1
    if ((number>>bit)&1)
        glob += 1
    if (((~number)>>(bit+1))&1)
        glob += 1
    if (number & (1<<(bit-1)))
        glob += 1
}
noInline(doTest);

for (let i=0; i<(1<<30); ++i)
    doTest(i, 15)

if (glob != 536870912)
    throw "Error: bad result: " + glob;
