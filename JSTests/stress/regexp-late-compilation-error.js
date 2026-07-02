function testRegExp1()
{
    /((a{100000000})*b{2100000000})+/.test("b");
}

function testRegExp2()
{
    /(a{1000000000}b{1000000000}|c{10009s0000}|)d{1094967295}e{1500000000}/.test("abcde");
}

function test(testRE)
{
    for (let i = 0; i < 5000; ++i) {
        try {
            testRE();
        } catch {};
    }
}

test(testRegExp1);
test(testRegExp2);
