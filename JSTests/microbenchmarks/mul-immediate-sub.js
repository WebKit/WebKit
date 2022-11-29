//@ skip if not $jitTests
//@ $skipModes << :lockdown if $architecture == "mips"
function doTest(max) {
    let sum = 0
    for (let i=0; i<max; ++i) {
        sum = (((((((((((sum|0) + ((i*256)|0))|0) - ((i*9)|0))|0) - ((i*31)|0))|0) - ((i*67)|0))|0) - ((i*64)|0))|0)
    }
    return sum
}
noInline(doTest);

for (let i=0; i<100000; ++i) doTest(10000)

if (doTest(1000) != 42457500)
    throw "Error: bad result: " + doTest(1000);
