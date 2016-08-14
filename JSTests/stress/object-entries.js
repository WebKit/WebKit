var obj = Object.create({ a: "qux", d: "qux" });
obj.a = "foo"; obj.b = "bar"; obj.c = "baz";
var entries = Object.entries(obj);
var passed = Array.isArray(entries) 
    && String(entries[0]) === "a,foo"
    && String(entries[1]) === "b,bar"
    && String(entries[2]) === "c,baz";

if (!passed)
    throw new Error("Object.entries return wrong result.");

var complexObject = {
    obj : {
        a: 'x',
        b: 'y'
    },
    primitive : 'z'
};

passed = false;
entries = Object.entries(complexObject);

passed = entries.length === 2 
    && entries[0][0] === 'obj' 
    && entries[0][1].a === 'x' 
    && entries[0][1].b === 'y' 
    && entries[1][0] === 'primitive' 
    && entries[1][1] === 'z';

if (!passed)
    throw new Error("Object.entries return wrong result.");

entries = Object.entries({ a: 'abcdef' });

passed = entries.length === 1 
    && entries[0][0] === 'a'
    && entries[0][1] === 'abcdef';

if (!passed)
    throw new Error("Object.entries return wrong result.");

var primitives = [
    ["string", [[0, 's'], [1, 't'], [2, 'r'], [3, 'i'], [4, 'n'], [5, 'g']]],
    [42, []],
    [Symbol("symbol"), []],
    [true, []],
    [false, []]
];

function compare(ax, bx) {
    if (ax.length !== bx.length)
        return false;
    for (var i = 0, iz = ax.length; i < iz; ++i) {
        if (String(ax[i]) !== String(bx[i]))
            return false;
    }
    return true;
}

for (var [primitive, expected] of primitives) {
    var ret = Object.entries(primitive);
    if (!compare(ret, expected))
        throw new Error("bad value for " + String(primitive) + ": " + String(ret));
}

[
    [ null, "TypeError: Object.entries requires that input parameter not be null or undefined" ],
    [ undefined, "TypeError: Object.entries requires that input parameter not be null or undefined" ]
].forEach(function ([value, message]) {
    var error = null;
    try {
        Object.entries(value);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("error not thrown");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
});

const getInvokedFunctions = (obj) => {
    let arr = []
    let p = new Proxy(obj, {
        ownKeys: function(...args) {
            arr.push("ownKeys");
            return Reflect.ownKeys(...args);
        },
        getOwnPropertyDescriptor: function(...args) {
            arr.push("getOwnPropertyDescriptor");
            return Reflect.getOwnPropertyDescriptor(...args);
        }
    });

    Object.entries(p);
    return arr;
};

const arr1 = getInvokedFunctions({});
passed = arr1.length === 1 && arr1[0] === "ownKeys";

if (!passed)
    throw new Error("Object.entries should invoke ownkeys.");

const arr2 = getInvokedFunctions({a:'foo', b:'boo'});
passed = arr2.length === 3 && arr2[0] === "ownKeys";

if (!passed)
    throw new Error("Object.entries should invoke ownkeys.");

passed = arr2[1] === "getOwnPropertyDescriptor";

if (!passed)
    throw new Error("Object.entries should get property descriptor.");

Array.prototype.push = function () { throw new Error("Array.prototype.push should not be used during invoking of Object.entries.")};
Object.getOwnPropertyDescriptor = function () { throw new Error("Array.prototype.getOwnPropertyDescriptor should not be used during invoking of Object.entries.")};

entries = Object.entries({a:'1-2', b:'3-4'});
passed = Array.isArray(entries) && String(entries[0]) === "a,1-2" && String(entries[1]) === "b,3-4";

if (!passed)
    throw new Error("Object.entries return wrong result.");
