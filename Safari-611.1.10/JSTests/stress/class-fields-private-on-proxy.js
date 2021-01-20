//@ requireOptions("--usePrivateClassFields=1")

function assert(a, e, m) {
    if (a !== e)
        throw new Error(m);
}

function assertArrayContent(a, e) {
    assert(a.length, e.length, "Size of arrays doesn't match");
    for (var i = 0; i < a.length; i++)
        assert(a[i], e[i], "a[" + i + "] = " + a[i] + " but e[" + i + "] = " + e[i]);
}

let arr = [];

class ProxyBase {
    constructor() {
        return new Proxy(this, {
            get: function (obj, prop) {
                arr.push(prop);
                return obj[prop];
            }
        });
    }
}

class Test extends ProxyBase {
    #f = 3;
    method() {
        return this.#f;
    }
}

let t = new Test();
let r = t.method();
assert(r, 3, "Expected: 3 but got: " + r);

assertArrayContent(arr, ['method']);

