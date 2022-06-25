// Regression test for https://bugs.webkit.org/show_bug.cgi?id=157246.
// Needs the browser needsSiteSpecificQuirks settings flag to be set in order to pass.

function assert(result) {
    if (!result)
        throw new Error("Unexpected result");
}

function makeObj(propName) {
    var o = {
        [propName]: function () { }, // accessed via get_by_val
        'c | d': function() { } // accessed via get_by_id
    }
    return o;
}

function test(obj) {
    for (let prop in obj) {
        let bodyStr = "return function " + obj[prop].name + "() { return 5; }";
        let func = new Function(bodyStr);
        assert(func()() === 5);
    }
}

test(makeObj('a | b'));
