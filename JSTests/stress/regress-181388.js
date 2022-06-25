function assert(x) {
    if (!x)
        throw "FAIL";
}

(function() {
    var trace = [];

    var foo = {
        value: 5,
        get bar() {
            trace.push("get");
            return this.value;
        },
        set bar(x) {
            throw "Should not be reached";
        },
        set bar(x) {
            trace.push("set2");
            this.value = x + 10000;
            return this.value;
        }
    }

    assert(foo.value == 5);
    assert(trace == "");
    assert(foo.bar == 5);
    assert(trace == "get");

    foo.bar = 20;
    assert(trace == "get,set2");

    assert(foo.value == 10020);
    assert(trace == "get,set2");
    assert(foo.bar == 10020);
    assert(trace == "get,set2,get");
})();

(function() {
    var trace = [];

    var foo = {
        value: 5,
        set bar(x) {
            trace.push("set");
            this.value = x;
            return this.value;
        },
        get bar() {
            throw "Should not be reached";
        },
        get bar() {
            trace.push("get2");
            this.value += 10000;
            return this.value;
        },
    }

    assert(foo.value == 5);
    assert(trace == "");
    assert(foo.bar == 10005);
    assert(trace == "get2");

    foo.bar = 20;
    assert(trace == "get2,set");

    assert(foo.value == 20);
    assert(trace == "get2,set");
    assert(foo.bar == 10020);
    assert(trace == "get2,set,get2");
})();
