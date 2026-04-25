function ensurePromise(expr) {
    var p;
    try {
        p = eval(expr);
    } catch (e) {
        testFailed("evaluating " + expr + " threw exception " + e);
        return null;
    }

    if (!(p instanceof Promise)) {
        testFailed(expr + " does not evaluate to a promise.");
        return null;
    }

    return p;
}

function promiseShouldResolve(expr) {
    return new Promise(function (done) {
        var p = ensurePromise(expr);
        if (!p) {
            done();
            return;
        }

        p.then(function (value) {
            testPassed("promise " + expr + " fulfilled with " + value);
            done();
        })
        .catch(function (reason) {
            testFailed("promise " + expr + " rejected unexpectedly:" + reason);
            done();
        });
    });
}

function promiseShouldReject(expr, reasonArg) {
    return new Promise(function (done) {
        var p = ensurePromise(expr);
        if (!p) {
            done();
            return;
        }

        p.then(function () {
            testFailed("promise " + expr + " fulfilled unexpectedly.");
            done();
        })
        .catch(function (actualReason) {
            if (!reasonArg) {
                testPassed("promise " + expr + " rejected with " + actualReason);
            } else {
                var reasonValue;
                try {
                    reasonValue = eval(reasonArg);
                } catch (ex) {
                    debug("promiseShouldReject: Error evaluating reason: " + ex);
                }

                if (actualReason == reasonValue)
                    testPassed("promise " + expr + " rejected with " + actualReason);
                else
                    testFailed("promise " + expr + " rejected with " + actualReason + "; expected reason " + reasonValue);
            }

            done();
        });
    });
}

function promiseShouldNotRejectWithTypeError(expr) {
    return new Promise(function (done) {
        var p = ensurePromise(expr);
        if (!p) {
            done();
            return;
        }

        p.then(function () {
            testPassed("promise " + expr + " did not reject with TypeError.");
            done();
        })
        .catch(function (reason) {
            if (reason instanceof TypeError)
                testFailed("promise " + expr + " rejected with TypeError");
            else
                testPassed("promise " + expr + " did not reject with TypeError.");
            done();
        });
    });
}
