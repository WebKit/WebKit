//@ skip if not $jitTests
//@ $skipModes << :lockdown
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
