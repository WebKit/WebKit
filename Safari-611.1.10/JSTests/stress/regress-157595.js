// Test that an overridden global on a RegExp object doesn't cause an infinite loop
// in String.match(). Instead it should eventually throw an Out of Memory exception.
// Skipping until the test doesn't timeout on the bots - <https://bugs.webkit.org/show_bug.cgi?id=157903>
//@ skip

class MyRegExp extends RegExp {
    constructor(pattern) {
        super(pattern, "");
    }

    get global() {
        return true;
    }
};

function test()
{
    let r = new MyRegExp(".");

    return "abc".match(r);
}

try {
    test();
} catch(e) {
    if (e.message != "Out of memory")
        throw "Wrong error: " + e;
}
