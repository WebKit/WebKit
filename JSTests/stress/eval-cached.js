(function () {
    "use strict";

    function verify() {
        for (var i = 0; i < counter; ++i) {
            if (results[i] != i)
                throw "strict mode verify() failed for item " + i + "."
        }
    }

    let results = [ ];
    let counter = 0;

    let x = counter++;
    results.push(eval("x"));

    {
        let x = counter++;
        results.push(eval("x"));
    }

    try {
        throw counter++;
    } catch (x) {
        results.push(eval("x"));
    }

    (() => {
        var x = counter++;
        results.push(eval("x"));
    })();

    (function (x) {
        results.push(eval("x"));
    })(counter++);

    verify();
})();

(function () {
    function verify() {
        for (var i = 0; i < counter; ++i) {
            if (results[i] != i)
                throw "non-strict mode verify() failed for item " + i + "."
        }
    }

    let results = [ ];
    let counter = 0;

    let x = counter++;
    results.push(eval("x"));

    {
        let x = counter++;
        results.push(eval("x"));
    }

    try {
        throw counter++;
    } catch (x) {
        results.push(eval("x"));
    }

    (() => {
        var x = counter++;
        results.push(eval("x"));
    })();

    (function (x) {
        results.push(eval("x"));
    })(counter++);

    with ({ x : counter++ }) {
        results.push(eval("x"));
    }

    verify();
})();
