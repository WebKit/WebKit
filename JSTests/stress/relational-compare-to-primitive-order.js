function lt(a, b) { return a < b; }
function le(a, b) { return a <= b; }
function gt(a, b) { return a > b; }
function ge(a, b) { return a >= b; }
var comparisons = [lt, le, gt, ge];
for (var f of comparisons)
    noInline(f);

function shouldThrowTypeError(f, a, b) {
    try {
        f(a, b);
    } catch (e) {
        if (!(e instanceof TypeError))
            throw e;
        return;
    }
    throw new Error(f.name + " did not throw");
}

var log;
function loggingObject(name, value) {
    return { valueOf() { log.push(name); return value; } };
}

class CustomError extends Error { }
function throwingObject() {
    return { valueOf() { throw new CustomError(); } };
}

function shouldThrowCustomError(f, a, b) {
    try {
        f(a, b);
    } catch (e) {
        if (!(e instanceof CustomError))
            throw e;
        return;
    }
    throw new Error(f.name + " did not throw");
}

for (var i = 0; i < testLoopCount / 16; ++i) {
    for (var f of comparisons) {
        for (var value of [1, "s", 1n]) {
            log = [];
            shouldThrowTypeError(f, Symbol(), loggingObject("right", value));
            if (log.join() !== "right")
                throw new Error(f.name + "(Symbol, object) converted [" + log + "]");

            log = [];
            shouldThrowTypeError(f, loggingObject("left", value), Symbol());
            if (log.join() !== "left")
                throw new Error(f.name + "(object, Symbol) converted [" + log + "]");
        }

        shouldThrowTypeError(f, 1n, Symbol());
        shouldThrowTypeError(f, Symbol(), 1n);
        shouldThrowCustomError(f, Symbol(), throwingObject());
        shouldThrowCustomError(f, throwingObject(), Symbol());

        log = [];
        f(loggingObject("left", 1), loggingObject("right", 2));
        if (log.join() !== "left,right")
            throw new Error(f.name + "(object, object) converted [" + log + "]");
    }
}
