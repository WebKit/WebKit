function assert(b) {
    if (!b)
        throw new Error("Bad assertion!");
}

function test(f) {
    for (let i = 0; i < 1000; i++)
        f();
}

function shallowEq(a, b) {
    if (a.length !== b.length)
        return false;
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            return false;
    }
    return true;
}

test(function() {
    let delProps = new Set;
    let hasProps = new Set;
    let getProps = new Set;
    let target = [ , , 1, , 4];
    let handler = {
        get(theTarget, key) {
            getProps.add(key);
            return Reflect.get(theTarget, key);
        },
        has(theTarget, key) {
            hasProps.add(key);
            return Reflect.has(theTarget, key);
        },
        deleteProperty(theTarget, key)
        {
            delProps.add(key);
            return Reflect.deleteProperty(theTarget, key);
        }
    };

    let proxy = new Proxy(target, handler);
    proxy.unshift(20);

    assert(delProps.size === 3);
    assert(delProps.has("1"));
    assert(delProps.has("2"));
    assert(delProps.has("4"));

    assert(hasProps.size === 5);
    assert(hasProps.has("0"));
    assert(hasProps.has("1"));
    assert(hasProps.has("2"));
    assert(hasProps.has("3"));
    assert(hasProps.has("4"));

    assert(getProps.size === 4);
    assert(getProps.has("unshift"));
    assert(getProps.has("length"));
    assert(getProps.has("2"));
    assert(getProps.has("4"));
});

test(function() {
    let delProps = new Set;
    let hasProps = new Set;
    let getProps = new Set;
    let target = [ 0, 0, , 1, , 4];
    let handler = {
        get(theTarget, key) {
            getProps.add(key);
            return Reflect.get(theTarget, key);
        },
        has(theTarget, key) {
            hasProps.add(key);
            return Reflect.has(theTarget, key);
        },
        deleteProperty(theTarget, key)
        {
            delProps.add(key);
            return Reflect.deleteProperty(theTarget, key);
        }
    };

    let proxy = new Proxy(target, handler);
    proxy.shift();
    assert(target.length === 5);

    assert(delProps.size === 3);
    assert(delProps.has("1"));
    assert(delProps.has("3"));
    assert(delProps.has("5"));

    assert(hasProps.size === 5);
    assert(hasProps.has("1"));
    assert(hasProps.has("2"));
    assert(hasProps.has("3"));
    assert(hasProps.has("4"));
    assert(hasProps.has("5"));

    assert(getProps.size === 6);
    assert(getProps.has("shift"));
    assert(getProps.has("length"));
    assert(getProps.has("0"));
    assert(getProps.has("1"));
    assert(getProps.has("3"));
    assert(getProps.has("5"));
});

test(function() {
    let delProps = new Set;
    let hasProps = new Set;
    let getProps = new Set;
    let target = [ 0, , 1, , 2];
    let handler = {
        get(theTarget, key) {
            getProps.add(key);
            return Reflect.get(theTarget, key);
        },
        has(theTarget, key) {
            hasProps.add(key);
            return Reflect.has(theTarget, key);
        },
        deleteProperty(theTarget, key)
        {
            delProps.add(key);
            return Reflect.deleteProperty(theTarget, key);
        }
    };

    let proxy = new Proxy(target, handler);
    proxy.splice(2, 2);

    assert(delProps.size === 2);
    assert(delProps.has("3"));
    assert(delProps.has("4"));

    assert(hasProps.size === 3);
    assert(hasProps.has("2"));
    assert(hasProps.has("3"));
    assert(hasProps.has("4"));

    assert(getProps.size === 4);
    assert(getProps.has("splice"));
    assert(getProps.has("length"));
    assert(getProps.has("2"));
    assert(getProps.has("4"));
});

test(function() {
    let x = [1,2,3];
    x.__proto__ = new Proxy([], {
        get(theTarget, prop, receiver) {
            assert(prop === "shift");
            assert(receiver === x);
            return Reflect.get(theTarget, prop);
        }
    });
    x.shift();
    assert(x.length === 2);
    assert(x[0] === 2);
    assert(x[1] === 3);
});
