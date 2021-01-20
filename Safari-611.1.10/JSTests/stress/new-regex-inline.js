//@ skip if $architecture == "x86"

function assert(a) {
    if (!a)
        throw Error("bad assertion");
}

function testRegexpInline(functor) {
    for (let i = 0; i < 100000; i++) {
        functor();
    }

    gc();

    // Create objects to force collected objects be reused
    for (let i = 0; i < 10000000; i++) {
        let a = {value: i};
    }

    // Checking if RegExp were collected
    for (let i = 0; i < 100; i++) {
        functor();
    }
}

function toInlineGlobal() {
    var re = /cc+/;

    assert(re.test("ccc"));
    assert(!re.test("abc"));
    return 0;
}

function withRegexp() {
    toInlineGlobal();
    var re = /(ab)+/;
    assert(re.test("ab"));
    assert(!re.test("ba"));
    return 0;
}

noInline(withRegexp);

testRegexpInline(withRegexp);

function inlineRegexpNotGlobal() {
    let toInline = () => {
        let re = /a+/;

        assert(re.test("aaaaaa"));
        assert(!re.test("bc"));
    }

    toInline();
}

noInline(inlineRegexpNotGlobal);

testRegexpInline(inlineRegexpNotGlobal);

function toInlineRecursive(depth) {
    if (depth == 5) {
        return;
    }

    var re = /(ef)+/;

    assert(re.test("efef"));
    assert(!re.test("abc"));
    
    toInlineRecursive(depth + 1);
}

function regexpContainsRecursive() {
    var re = /r+/;
    toInlineRecursive(0);

    assert(re.test("r"));
    assert(!re.test("ab"));
}
noInline(regexpContainsRecursive);

testRegexpInline(regexpContainsRecursive);

