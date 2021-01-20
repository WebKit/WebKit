// Regression test for bug 179140.

function testWithoutFTL()
{
    g=() => 0
    f=(a) => g.apply(0,a)

    noFTL(f);

    for(i=1e6;i--;)
        f([])

    try {
        f({length:1e10})
    } catch(e) {
        if (!(e instanceof RangeError))
            throw "Expected RangeError due to stack overflow";
    }
}

function testWithFTL()
{
    g=() => 0
    f=(a) => g.apply(0,a)

    for(i=1e6;i--;)
        f([])

    try {
        f({length:1e10})
    } catch(e) {
        if (!(e instanceof RangeError))
            throw "Expected RangeError due to stack overflow";
    }
}

testWithoutFTL();
testWithFTL();
