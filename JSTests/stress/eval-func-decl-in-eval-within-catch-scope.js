var err = new Error();

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
    }
})();

shouldThrow(function() {
    var e = 2;
    try {
        throw err;
    } catch ({e}) {
        eval(`function e() { return 1; }`); // syntax error
    }
}, "SyntaxError: Can't create duplicate variable in eval: 'e'");
