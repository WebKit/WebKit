function assert(b) {
    if (!b)
        throw new Error("Bad assertion!");
}

function test(f) {
    for (let i = 0; i < 500; i++)
        f();
}

test(function() {
    let foo = "hello";
    let threw = false;
    try {
        foo.endsWith(/foo/);
    } catch(e) {
        assert(e.toString() === "TypeError: Argument to String.prototype.endsWith cannot be a RegExp");
        threw = true;
    }
    assert(threw);
});

test(function() {
    let foo = "hello";
    let threw = false;
    try {
        foo.startsWith(/foo/);
    } catch(e) {
        assert(e.toString() === "TypeError: Argument to String.prototype.startsWith cannot be a RegExp");
        threw = true;
    }
    assert(threw);
});

test(function() {
    let foo = "hello";
    let threw = false;
    try {
        foo.includes(/foo/);
    } catch(e) {
        assert(e.toString() === "TypeError: Argument to String.prototype.includes cannot be a RegExp");
        threw = true;
    }
    assert(threw);
});

test(function() {
    let props = [];
    let proxy = new Proxy(/foo/, {
        get(theTarget, prop) {
            props.push(prop);
            return theTarget[prop];
        }
    });

    let foo = "hello";
    let threw = false;
    try {
        foo.endsWith(proxy);
    } catch(e) {
        assert(e.toString() === "TypeError: Argument to String.prototype.endsWith cannot be a RegExp");
        threw = true;
    }
    assert(threw);
    assert(props.length === 1);
    assert(props[0] === Symbol.match);
});

test(function() {
    let props = [];
    let proxy = new Proxy(/foo/, {
        get(theTarget, prop) {
            props.push(prop);
            return theTarget[prop];
        }
    });

    let foo = "hello";
    let threw = false;
    try {
        foo.startsWith(proxy);
    } catch(e) {
        assert(e.toString() === "TypeError: Argument to String.prototype.startsWith cannot be a RegExp");
        threw = true;
    }
    assert(threw);
    assert(props.length === 1);
    assert(props[0] === Symbol.match);
});

test(function() {
    let props = [];
    let proxy = new Proxy(/foo/, {
        get(theTarget, prop) {
            props.push(prop);
            return theTarget[prop];
        }
    });

    let foo = "hello";
    let threw = false;
    try {
        foo.includes(proxy);
    } catch(e) {
        assert(e.toString() === "TypeError: Argument to String.prototype.includes cannot be a RegExp");
        threw = true;
    }
    assert(threw);
    assert(props.length === 1);
    assert(props[0] === Symbol.match);
});

test(function() {
    let props = [];
    let proxy = new Proxy(/foo/, {
        get(theTarget, prop) {
            props.push(prop);
            if (prop === Symbol.match)
                return undefined;
            return theTarget[prop];
        }
    });

    let foo = "/foo/";
    let threw = false;
    let result = foo.includes(proxy);
    assert(result);
    assert(props.length === 5);
    assert(props[0] === Symbol.match);
    assert(props[1] === Symbol.toPrimitive);
    assert(props[2] === "toString");
    assert(props[3] === "source");
    assert(props[4] === "flags");
});

test(function() {
    let props = [];
    let proxy = new Proxy(/foo/, {
        get(theTarget, prop) {
            props.push(prop);
            if (prop === Symbol.match)
                return undefined;
            return theTarget[prop];
        }
    });

    let foo = "/foo/";
    let threw = false;
    let result = foo.startsWith(proxy);
    assert(result);
    assert(props.length === 5);
    assert(props[0] === Symbol.match);
    assert(props[1] === Symbol.toPrimitive);
    assert(props[2] === "toString");
    assert(props[3] === "source");
    assert(props[4] === "flags");
});

test(function() {
    let props = [];
    let proxy = new Proxy(/foo/, {
        get(theTarget, prop) {
            props.push(prop);
            if (prop === Symbol.match)
                return undefined;
            return theTarget[prop];
        }
    });

    let foo = "/foo/";
    let threw = false;
    let result = foo.endsWith(proxy);
    assert(result);
    assert(props.length === 5);
    assert(props[0] === Symbol.match);
    assert(props[1] === Symbol.toPrimitive);
    assert(props[2] === "toString");
    assert(props[3] === "source");
    assert(props[4] === "flags");
});
