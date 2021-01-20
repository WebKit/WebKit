function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

shouldThrow(() => {
    Function("@", { toString() { throw 42; } })
}, `42`);

var counter = 0;
class Parameter {
    constructor(index)
    {
        this.index = index;
    }

    toString() {
        shouldBe(this.index, counter);
        counter++;
        return `x${this.index}`;
    }
};

class Body {
    constructor(index)
    {
        this.index = index;
    }

    toString() {
        shouldBe(this.index, counter);
        counter++;
        return `42`;
    }
};

var parameters = [];
for (var i = 0; i < 50; ++i) {
    parameters.push(new Parameter(parameters.length));
    var args = parameters.slice();
    args.push(new Body(args.length));
    counter = 0;
    Function.apply(this, args);
    shouldBe(counter, args.length);
}
