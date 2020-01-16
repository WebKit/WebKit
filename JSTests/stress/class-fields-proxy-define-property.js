//@ requireOptions("--useClassFields=1")
//@ defaultNoEagerRun

function assert(a, e, m) {
    m = m || "Expected: " + e + " but got: " + a;
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
            defineProperty: function (target, key, descriptor) {
                arr.push(key);
                assert(descriptor.enumerable, true);
                assert(descriptor.configurable, true);
                assert(descriptor.writable, true);
                return Reflect.defineProperty(target, key, descriptor);
            }
        });
    }
}

class Test extends ProxyBase {
  f = 3;
  g = "test";
}

let t = new Test();
assert(t.f, 3);
assert(t.g, "test");

assertArrayContent(arr, ["f", "g"]);
