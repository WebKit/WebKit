//@ requireOptions("--jitPolicyScale=0", "--useConcurrentJIT=0")
// This tests that when a switch(String) converts the String argument, it properly handles OOM

function test(createOOMString)
{
    var str = String.fromCharCode(365);
    if (createOOMString)
        str = str.padEnd(2147483644, '123');

    switch (str) {
    case "one":
        throw "Case \"one\", dhouldn't get here";
        break;
    case "two": 
        throw "Case \"two\", shouldn't get here";
        break;
    case "three":
        throw "Case \"three\", shouldn't get here";
        break;
    default:
        if (createOOMString)
            throw "Default case, shouldn't get here";
        break;
    }
}

function testLowerTiers()
{
    for (let i = 0; i < 200; i++) {
        try {
            test(true);
        } catch(e) {
            if (e != "Error: Out of memory")
                throw "Unexpecte error: \"" + e + "\"";
        }
    }
}

function testFTL()
{
    for (let i = 0; i < 1000; i++) {
        try {
            test(i >= 50);
        } catch(e) {
            if (e != "Error: Out of memory")
                throw "Unexpecte error: \"" + e + "\"";
        }
    }
}

testLowerTiers();
testFTL();
