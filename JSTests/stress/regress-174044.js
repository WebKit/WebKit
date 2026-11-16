// Regression test for https://bugs.webkit.org/show_bug.cgi?id=174044.  This test should not throw or crash.

function test1()
{
    let expected = ["\na", "\na", "\na", "\n"];

    let str = "\na\na\na\n";
    let re = new RegExp(".*\\s.*", "g");

    let match = str.match(re);

    if (match.length != expected.length)
        throw "Expected match.length of " + expected.length + ", got " + match.length;

    for (let i = 0; i < expected.length; i++) {
        if (match[i] != expected[i])
            throw "Expected match[" + i + "] to be \"" + expected[i] + "\", got \"" + match[i] + "\"";
    }
}

function test2()
{
    let result = undefined;

    let re = new RegExp(".*\\s.*", "g");
    let str = "\na\n";
    result = str.replace(re,'x');

    if (result != "xx")
        throw "Expected result of \"xx\", got \"" + result + "\"";
}

for (let i = 0; i < 5000; i++)
    test1();

for (let i = 0; i < 5000; i++)
    test2();
