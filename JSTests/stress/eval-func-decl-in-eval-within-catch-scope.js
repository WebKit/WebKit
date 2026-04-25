var err = new Error();
err.e = "foo";

function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

function shouldThrow(func, errorMessage) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!error)
        throw new Error(`Didn't throw!`);
    if (String(error) !== errorMessage)
        throw new Error(`Bad error: ${error}`);
}

(function() {
    var e = 1;
    try {
        throw err;
    } catch (e) {
        eval(`function e() { return 1; }`); // no error
        assert(e === err);
    }
    assert(e() === 1);
})();

(function() {
    var e = 1;
    try {
        throw err;
    } catch (e) {
        eval(`if (true) { function e() { return 1; } }`); // no error
        assert(e === err);
    }
    assert(e() === 1);
})();

(function() {
    var e = 1;
    try {
        throw err;
    } catch ({e}) {
        eval(`if (true) { function e() { return 1; } }`); // no error
        assert(e === "foo");
    }
    assert(e === 1);
})();

shouldThrow(function() {
    var e = 2;
    try {
        throw err;
    } catch ({e}) {
        eval(`function e() { return 1; }`); // syntax error
    }
}, "SyntaxError: Can't create duplicate variable in eval: 'e'");

shouldThrow(function() {
    var e = 2;
    try {
        throw err;
    } catch ({e}) {
        eval(`var e = 1;`); // syntax error
    }
}, "SyntaxError: Can't create duplicate variable in eval: 'e'");

shouldThrow(function() {
    try {
        throw err;
    } catch ({...e}) {
        eval(`var e;`); // syntax error
    }
}, "SyntaxError: Can't create duplicate variable in eval: 'e'");
