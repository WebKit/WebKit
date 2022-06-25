//@ skip if not $jitTests
let glob = 0

function doTest(number) {
    if ((number>>16)&1)
        glob += 1
    if ((number>>15)&1)
        glob -= 1
    if ((number>>18)&1)
        glob += 1
    if ((number>>19)&1)
        glob += 1
}
noInline(doTest);

for (let i=0; i<(1<<30); ++i)
    doTest(i)

if (glob != 1073741824)
    throw "Error: bad result: " + glob;
