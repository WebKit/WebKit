function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}

function test1() {
    let boundFunction = function () {}.bind();
    Object.defineProperty(boundFunction, "prototype", {
        get() {
            throw Error("Hello");
        }
    });

    let threw = false;
    try {
        Reflect.construct(function() {}, [], boundFunction);
    } catch(e) {
        threw = true;
        assert(e.message === "Hello");
    }
    assert(threw);
}
test1();

function test2() {
    let boundFunction = function () {}.bind();
    let counter = 0;
    Object.defineProperty(boundFunction, "prototype", {
        get() {
            ++counter;
            return {};
        }
    });

    const iters = 1000;
    for (let i = 0; i < iters; ++i)
        Reflect.construct(function() {}, [], boundFunction);
    assert(counter === iters);
}
test2();
