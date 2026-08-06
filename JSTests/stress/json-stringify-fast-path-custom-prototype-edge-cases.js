//@ requireOptions("--useDollarVM=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldThrow(func, errorMessage) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (String(error) !== errorMessage)
            throw new Error("bad error: " + String(error));
    }
    if (!errorThrown)
        throw new Error("not thrown");
}

// Warm the fast path against an instance so its structure caches "no toJSON", then mutate.
function warm(value, expected) {
    for (let i = 0; i < 20; ++i)
        shouldBe(JSON.stringify(value), expected);
}

// toJSON appears on the prototype through every route that can add a property.
{
    class C { constructor() { this.x = 1; } }
    let c = new C;
    warm(c, `{"x":1}`);
    C.prototype.toJSON = () => "assign";
    shouldBe(JSON.stringify(c), `"assign"`);
    delete C.prototype.toJSON;
    warm(c, `{"x":1}`);
    Object.defineProperty(C.prototype, "toJSON", { value: () => "define", configurable: true, enumerable: false, writable: true });
    shouldBe(JSON.stringify(c), `"define"`);
    delete C.prototype.toJSON;
    warm(c, `{"x":1}`);
    Object.assign(C.prototype, { toJSON: () => "Object.assign" });
    shouldBe(JSON.stringify(c), `"Object.assign"`);
    delete C.prototype.toJSON;
    warm(c, `{"x":1}`);
    Reflect.set(C.prototype, "toJSON", () => "Reflect.set");
    shouldBe(JSON.stringify(c), `"Reflect.set"`);
    delete C.prototype.toJSON;
    warm(c, `{"x":1}`);
    C.prototype["to" + "JSON"] = () => "computed";
    shouldBe(JSON.stringify(c), `"computed"`);
}

// toJSON replaced in place (same structure, different value).
{
    class C { constructor() { this.x = 1; } toJSON() { return "v1"; } }
    let c = new C;
    warm(c, `"v1"`);
    C.prototype.toJSON = () => "v2";
    shouldBe(JSON.stringify(c), `"v2"`);
    C.prototype.toJSON = undefined;
    shouldBe(JSON.stringify(c), `{"x":1}`);
    C.prototype.toJSON = null;
    shouldBe(JSON.stringify(c), `{"x":1}`);
    C.prototype.toJSON = 42;
    shouldBe(JSON.stringify(c), `{"x":1}`);
    C.prototype.toJSON = () => "v3";
    shouldBe(JSON.stringify(c), `"v3"`);
}

// toJSON as accessor on the prototype: getter must run once per serialization, with the instance as receiver.
{
    let receivers = [];
    class C { constructor() { this.x = 1; } }
    Object.defineProperty(C.prototype, "toJSON", { get() { receivers.push(this); return function () { return this.x * 10; }; }, configurable: true });
    let a = new C, b = new C;
    b.x = 2;
    shouldBe(JSON.stringify([a, b, a]), `[10,20,10]`);
    shouldBe(receivers.length, 3);
    shouldBe(receivers[0], a);
    shouldBe(receivers[1], b);
    shouldBe(receivers[2], a);

    // Getter that mutates the holder mid-serialization.
    class D { constructor() { this.p = 1; this.q = 2; } }
    Object.defineProperty(D.prototype, "toJSON", { get() { delete this.q; this.r = 3; return undefined; }, configurable: true });
    shouldBe(JSON.stringify(new D), `{"p":1,"r":3}`);
}

// Prototype swapped after warm-up.
{
    class C { constructor() { this.x = 1; } }
    let c = new C;
    warm(c, `{"x":1}`);
    Object.setPrototypeOf(c, { toJSON() { return "swapped instance proto"; } });
    shouldBe(JSON.stringify(c), `"swapped instance proto"`);

    class D { constructor() { this.x = 1; } }
    let d = new D;
    warm(d, `{"x":1}`);
    Object.setPrototypeOf(D.prototype, { toJSON() { return "swapped grandparent"; } });
    shouldBe(JSON.stringify(d), `"swapped grandparent"`);
    Object.setPrototypeOf(D.prototype, Object.prototype);
    shouldBe(JSON.stringify(d), `{"x":1}`);
    Object.setPrototypeOf(D.prototype, null);
    shouldBe(JSON.stringify(d), `{"x":1}`);
}

// Deep chain: toJSON inserted at every level in turn.
{
    const depth = 12;
    let protos = [Object.create(null)];
    for (let i = 1; i < depth; ++i)
        protos.push(Object.create(protos[i - 1]));
    let leaf = Object.create(protos[depth - 1]);
    leaf.v = 1;
    warm({ leaf }, `{"leaf":{"v":1}}`);
    for (let i = 0; i < depth; ++i) {
        protos[i].toJSON = () => "level" + i;
        shouldBe(JSON.stringify({ leaf }), `{"leaf":"level${i}"}`);
        delete protos[i].toJSON;
        shouldBe(JSON.stringify({ leaf }), `{"leaf":{"v":1}}`);
    }
}

// Many distinct classes in one payload; toJSON added to one in the middle afterwards.
{
    let classes = [];
    for (let i = 0; i < 30; ++i)
        classes.push(eval(`(class K${i} { constructor() { this.i = ${i}; } })`));
    let payload = classes.map((K) => new K);
    let expected = "[" + classes.map((_, i) => `{"i":${i}}`).join(",") + "]";
    warm(payload, expected);
    classes[17].prototype.toJSON = function () { return -this.i; };
    shouldBe(JSON.stringify(payload), expected.replace(`{"i":17}`, `-17`));
}

// Same class, instances with divergent structures (property order / extra props / deletes).
{
    class C { constructor(flip) { if (flip) { this.b = 2; this.a = 1; } else { this.a = 1; this.b = 2; } } }
    let x = new C(false), y = new C(true), z = new C(false);
    z.c = 3;
    delete z.a;
    warm([x, y, z], `[{"a":1,"b":2},{"b":2,"a":1},{"b":2,"c":3}]`);
    C.prototype.toJSON = function () { return Object.keys(this).join(""); };
    shouldBe(JSON.stringify([x, y, z]), `["ab","ba","bc"]`);
}

// Dictionary-mode prototype and dictionary-mode instance.
{
    class C { constructor() { this.x = 1; } }
    for (let i = 0; i < 100; ++i)
        C.prototype["m" + i] = i;
    for (let i = 0; i < 100; ++i)
        delete C.prototype["m" + i];
    let c = new C;
    warm(c, `{"x":1}`);
    C.prototype.toJSON = () => "dict proto";
    shouldBe(JSON.stringify(c), `"dict proto"`);
    delete C.prototype.toJSON;
    warm(c, `{"x":1}`);

    $vm.toUncacheableDictionary(C.prototype);
    warm(c, `{"x":1}`);
    C.prototype.toJSON = () => "uncacheable dict proto";
    shouldBe(JSON.stringify(c), `"uncacheable dict proto"`);
    delete C.prototype.toJSON;
    shouldBe(JSON.stringify(c), `{"x":1}`);

    class D { constructor() { this.x = 1; } }
    let d = new D;
    for (let i = 0; i < 100; ++i)
        d["k" + i] = i;
    for (let i = 0; i < 100; ++i)
        delete d["k" + i];
    warm({ d }, `{"d":{"x":1}}`);
    D.prototype.toJSON = () => "dict instance";
    shouldBe(JSON.stringify({ d }), `{"d":"dict instance"}`);
}

// Frozen / sealed / non-extensible prototypes and instances.
{
    class C { constructor() { this.x = 1; } }
    Object.freeze(C.prototype);
    let c = new C;
    warm(c, `{"x":1}`);
    shouldBe(JSON.stringify(Object.freeze(new C)), `{"x":1}`);
    shouldBe(JSON.stringify(Object.seal(new C)), `{"x":1}`);
    shouldBe(JSON.stringify(Object.preventExtensions(new C)), `{"x":1}`);

    class D { constructor() { this.x = 1; } }
    Object.defineProperty(D.prototype, "toJSON", { value() { return "frozen toJSON"; }, writable: false, configurable: false });
    Object.freeze(D.prototype);
    shouldBe(JSON.stringify(new D), `"frozen toJSON"`);
}

// Objects in the chain whose property lookup is not side-effect-free must be observed.
{
    // Proxy as immediate prototype.
    let traps = [];
    let handler = {
        get(t, k, r) { traps.push("get:" + String(k)); return Reflect.get(t, k, r); },
        has(t, k) { traps.push("has:" + String(k)); return Reflect.has(t, k); },
        getOwnPropertyDescriptor(t, k) { traps.push("gopd:" + String(k)); return Reflect.getOwnPropertyDescriptor(t, k); },
        ownKeys(t) { traps.push("ownKeys"); return Reflect.ownKeys(t); },
        getPrototypeOf(t) { traps.push("getPrototypeOf"); return Reflect.getPrototypeOf(t); },
    };
    let o = Object.create(new Proxy({}, handler));
    o.a = 1;
    for (let i = 0; i < 5; ++i) {
        traps.length = 0;
        shouldBe(JSON.stringify({ o }), `{"o":{"a":1}}`);
        shouldBe(traps.join(","), `get:toJSON`);
    }

    // Proxy further up the chain, returning a toJSON.
    let mid = Object.create(new Proxy({}, { get(t, k) { if (k === "toJSON") return () => "from proxy"; } }));
    let leaf = Object.create(mid);
    leaf.b = 2;
    shouldBe(JSON.stringify({ leaf }), `{"leaf":"from proxy"}`);

    // Revoked proxy in the chain.
    let { proxy, revoke } = Proxy.revocable({}, {});
    let r = Object.create(proxy);
    r.c = 3;
    shouldBe(JSON.stringify({ r }), `{"r":{"c":3}}`);
    revoke();
    shouldThrow(() => JSON.stringify({ r }), "TypeError: Proxy has already been revoked. No more operations are allowed to be performed on it");

    // $vm ImpureGetter (overridesGetOwnPropertySlot) in the chain, delegating to an object with toJSON.
    let delegate = { toJSON() { return "impure"; } };
    let ig = Object.create($vm.createImpureGetter(delegate));
    ig.d = 4;
    shouldBe(JSON.stringify({ ig }), `{"ig":"impure"}`);
    let ig2 = Object.create($vm.createImpureGetter({}));
    ig2.d = 4;
    shouldBe(JSON.stringify({ ig2 }), `{"ig2":{"d":4}}`);
}

// Built-in prototypes with lazily reified static property tables in the chain.
{
    let g = createGlobalObject();
    let { Object: GO, Date: GD, JSON: GJ } = g;
    // Fresh realm so nothing has reified Date.prototype.toJSON yet.
    let o = GO.create(GD.prototype);
    o.x = 1;
    shouldThrow(() => GJ.stringify({ o }), "TypeError: Type error");
    shouldThrow(() => GJ.stringify(o), "TypeError: Type error");
    let oo = GO.create(GO.create(GD.prototype));
    oo.y = 2;
    shouldThrow(() => GJ.stringify({ oo }), "TypeError: Type error");

    // A static-table prototype without toJSON is fine (RegExp.prototype has a static table but no toJSON).
    let p = Object.create(RegExp.prototype);
    p.z = 3;
    warm({ p }, `{"p":{"z":3}}`);
    RegExp.prototype.toJSON = () => "regexp proto";
    shouldBe(JSON.stringify({ p }), `{"p":"regexp proto"}`);
    delete RegExp.prototype.toJSON;
}

// Non-final ObjectType cells with custom prototypes must keep slow-path semantics.
{
    class C { constructor() { this.x = 1; } }
    let bi = Object.setPrototypeOf(Object(1n), C.prototype);
    shouldThrow(() => JSON.stringify({ bi }), "TypeError: JSON.stringify cannot serialize BigInt.");
    C.prototype.toJSON = function () { return typeof this; };
    shouldBe(JSON.stringify({ bi }), `{"bi":"object"}`);
    delete C.prototype.toJSON;

    let sym = Object.setPrototypeOf(Object(Symbol("s")), C.prototype);
    shouldBe(JSON.stringify({ sym }), `{"sym":{}}`);

    let num = Object.setPrototypeOf(new Number(7), C.prototype);
    shouldBe(JSON.stringify({ num }), `{"num":null}`);
    let num2 = Object.setPrototypeOf(new Number(7), { valueOf() { return 8; } });
    shouldBe(JSON.stringify({ num2 }), `{"num2":8}`);

    let bool = Object.setPrototypeOf(new Boolean(false), C.prototype);
    shouldBe(JSON.stringify({ bool }), `{"bool":false}`);

    let str = Object.setPrototypeOf(new String("s"), { toString() { return "custom toString"; } });
    shouldBe(JSON.stringify({ str }), `{"str":"custom toString"}`);

    let raw = JSON.rawJSON("123");
    shouldBe(Object.getPrototypeOf(raw), null);
    shouldBe(JSON.stringify({ raw }), `{"raw":123}`);

    let err = new TypeError("m");
    shouldBe(JSON.stringify({ err }), `{"err":{}}`);
    let re = /x/;
    re.k = 1;
    shouldBe(JSON.stringify({ re }), `{"re":{"k":1}}`);
    let map = new Map;
    map.k = 1;
    shouldBe(JSON.stringify({ map }), `{"map":{"k":1}}`);
    let date = new Date(0);
    shouldBe(JSON.stringify({ date }), `{"date":"1970-01-01T00:00:00.000Z"}`);
}

// Callable / arguments / namespace-like objects with custom or null prototypes.
{
    let f = Object.setPrototypeOf(function () { }, null);
    f.x = 1;
    shouldBe(JSON.stringify({ f }), `{}`);
    shouldBe(JSON.stringify([f]), `[null]`);
    let bound = Object.setPrototypeOf((function () { }).bind(), { toJSON() { return "bound"; } });
    shouldBe(JSON.stringify({ bound }), `{"bound":"bound"}`);
    let args = (function () { return arguments; })(1, 2);
    Object.setPrototypeOf(args, null);
    shouldBe(JSON.stringify(args), `{"0":1,"1":2}`);
}

// Own-property shapes on custom-prototype instances that the fast path must reject or handle.
{
    class C { constructor() { this.x = 1; } }

    let indexed = new C;
    indexed[0] = "zero";
    shouldBe(JSON.stringify(indexed), `{"0":"zero","x":1}`);

    let withGetter = new C;
    let calls = 0;
    Object.defineProperty(withGetter, "g", { get() { calls++; return "got"; }, enumerable: true });
    shouldBe(JSON.stringify(withGetter), `{"x":1,"g":"got"}`);
    shouldBe(calls, 1);

    let withSymbol = new C;
    withSymbol[Symbol("s")] = 1;
    withSymbol.y = 2;
    shouldBe(JSON.stringify(withSymbol), `{"x":1,"y":2}`);

    let withNonEnumerable = new C;
    Object.defineProperty(withNonEnumerable, "hidden", { value: 1, enumerable: false });
    withNonEnumerable.y = 2;
    shouldBe(JSON.stringify(withNonEnumerable), `{"x":1,"y":2}`);

    let ownToJSON = new C;
    ownToJSON.toJSON = () => "own enumerable";
    shouldBe(JSON.stringify({ ownToJSON }), `{"ownToJSON":"own enumerable"}`);
    let ownHiddenToJSON = new C;
    Object.defineProperty(ownHiddenToJSON, "toJSON", { value: () => "own non-enumerable", enumerable: false });
    shouldBe(JSON.stringify({ ownHiddenToJSON }), `{"ownHiddenToJSON":"own non-enumerable"}`);
    let ownNonCallableToJSON = new C;
    ownNonCallableToJSON.toJSON = "not callable";
    shouldBe(JSON.stringify(ownNonCallableToJSON), `{"x":1,"toJSON":"not callable"}`);

    let undefinedAndFunctionValues = new C;
    undefinedAndFunctionValues.u = undefined;
    undefinedAndFunctionValues.fn = function () { };
    undefinedAndFunctionValues.s = Symbol();
    undefinedAndFunctionValues.y = 2;
    shouldBe(JSON.stringify(undefinedAndFunctionValues), `{"x":1,"y":2}`);

    let escapes = new C;
    escapes["key\n"] = "v\"\\";
    shouldBe(JSON.stringify(escapes), `{"x":1,"key\\n":"v\\"\\\\\\u0001"}`);

    let sixteenBit = new C;
    sixteenBit.name = "日本語";
    sixteenBit["キー"] = 1;
    shouldBe(JSON.stringify(sixteenBit), `{"x":1,"name":"日本語","キー":1}`);

    class Big { constructor() { for (let i = 0; i < 200; ++i) this["p" + i] = i; } }
    let big = new Big;
    shouldBe(JSON.stringify(big), JSON.stringify(Object.assign({}, big)));
    shouldBe(JSON.parse(JSON.stringify(big)).p199, 199);

    class P { #priv = 42; constructor() { this.pub = 1; } static has(o) { return #priv in o; } }
    shouldBe(JSON.stringify(new P), `{"pub":1}`);
    shouldBe(P.has(JSON.parse(JSON.stringify(new P))), false);
}

// Enumerable data on the prototype never leaks; shadowing works.
{
    let proto = { inherited: "no", shadowed: "proto" };
    let o = Object.create(proto);
    o.shadowed = "own";
    warm(o, `{"shadowed":"own"}`);
    Object.defineProperty(proto, "accessor", { get() { throw new Error("prototype getter must not run"); }, enumerable: true });
    shouldBe(JSON.stringify(o), `{"shadowed":"own"}`);
}

// Object.prototype.toJSON interacts with custom-prototype instances (chain ends at Object.prototype) but not null-prototype ones.
{
    class C { constructor() { this.x = 1; } }
    let c = new C;
    let n = Object.create(null);
    n.x = 1;
    let each = () => [c, n, {}].map((v) => JSON.stringify(v)).join("|");
    for (let i = 0; i < 20; ++i)
        shouldBe(each(), `{"x":1}|{"x":1}|{}`);
    Object.prototype.toJSON = function () { return "OP"; };
    shouldBe(each(), `"OP"|{"x":1}|"OP"`);
    delete Object.prototype.toJSON;
    for (let i = 0; i < 20; ++i)
        shouldBe(each(), `{"x":1}|{"x":1}|{}`);
    Object.defineProperty(Object.prototype, "toJSON", { get() { return () => "OP getter"; }, configurable: true });
    shouldBe(each(), `"OP getter"|{"x":1}|"OP getter"`);
    delete Object.prototype.toJSON;
    shouldBe(each(), `{"x":1}|{"x":1}|{}`);
}

// Cross-realm instances and prototypes.
{
    let g = createGlobalObject();
    g.eval(`var R = class R { constructor() { this.r = 1; } }; var inst = new R; var plain = { p: 1 };`);
    shouldBe(JSON.stringify({ a: g.inst, b: g.plain }), `{"a":{"r":1},"b":{"p":1}}`);
    shouldBe(g.JSON.stringify({ a: g.inst, b: g.plain }), `{"a":{"r":1},"b":{"p":1}}`);
    g.R.prototype.toJSON = () => "other realm";
    shouldBe(JSON.stringify({ a: g.inst }), `{"a":"other realm"}`);
    delete g.R.prototype.toJSON;
    g.Object.prototype.toJSON = () => "other realm OP";
    shouldBe(JSON.stringify({ a: g.inst, b: g.plain, c: {} }), `{"a":"other realm OP","b":"other realm OP","c":{}}`);
    delete g.Object.prototype.toJSON;

    let mixed = Object.create(g.inst);
    mixed.m = 1;
    shouldBe(JSON.stringify(mixed), `{"m":1}`);
    g.R.prototype.toJSON = () => "via other realm chain";
    shouldBe(JSON.stringify(mixed), `"via other realm chain"`);
}

// toJSON receives the key and correct receiver on the fast->slow handoff; result feeds back into serialization.
{
    let seen = [];
    class C { constructor(v) { this.v = v; } }
    C.prototype.toJSON = function (key) { seen.push(key); return { wrapped: this.v, nested: new D(this.v) }; };
    class D { constructor(v) { this.d = v; } }
    shouldBe(JSON.stringify({ a: new C(1), list: [new C(2)] }), `{"a":{"wrapped":1,"nested":{"d":1}},"list":[{"wrapped":2,"nested":{"d":2}}]}`);
    shouldBe(seen.join(","), `a,0`);
    D.prototype.toJSON = function (key) { seen.push("D:" + key); return this.d * 100; };
    shouldBe(JSON.stringify(new C(3)), `{"wrapped":3,"nested":300}`);
    shouldBe(seen.join(","), `a,0,,D:nested`);
}

// Interaction with gap / replacer arguments (replacer always takes the slow path; gap has its own fast path).
{
    class C { constructor() { this.x = 1; this.y = new D; } }
    class D { constructor() { this.z = 2; } }
    shouldBe(JSON.stringify(new C, null, 2), `{\n  "x": 1,\n  "y": {\n    "z": 2\n  }\n}`);
    shouldBe(JSON.stringify(new C, null, "--"), `{\n--"x": 1,\n--"y": {\n----"z": 2\n--}\n}`);
    shouldBe(JSON.stringify(new C, ["y", "z"]), `{"y":{"z":2}}`);
    shouldBe(JSON.stringify(new C, (k, v) => (v instanceof D ? "replaced" : v)), `{"x":1,"y":"replaced"}`);
    D.prototype.toJSON = () => "tj";
    shouldBe(JSON.stringify(new C, null, 1), `{\n "x": 1,\n "y": "tj"\n}`);
}

// Large payload that overflows the static buffer mid-way through custom-prototype instances, then toJSON added.
{
    class Row { constructor(i) { this.id = i; this.label = "row-" + i + "-".repeat(50); } }
    let rows = [];
    for (let i = 0; i < 400; ++i)
        rows.push(new Row(i));
    let s = JSON.stringify(rows);
    shouldBe(s.length > 8192, true);
    shouldBe(JSON.parse(s)[399].id, 399);
    Row.prototype.toJSON = function () { return this.id; };
    shouldBe(JSON.stringify(rows), "[" + rows.map((r) => r.id).join(",") + "]");
}

// Cyclic structure through custom-prototype instances still throws.
{
    class N { constructor() { this.next = null; } }
    let a = new N, b = new N;
    a.next = b;
    b.next = a;
    shouldThrow(() => JSON.stringify(a), "TypeError: JSON.stringify cannot serialize cyclic structures.");
}

// has-poly-proto structures (created by repeatedly instantiating a class defined inside a function).
{
    function make(v) {
        class Poly { constructor() { this.v = v; } }
        return new Poly;
    }
    let objs = [];
    for (let i = 0; i < 50; ++i)
        objs.push(make(i));
    shouldBe(JSON.stringify(objs), "[" + objs.map((o) => `{"v":${o.v}}`).join(",") + "]");
    Object.getPrototypeOf(objs[10]).toJSON = () => "poly10";
    shouldBe(JSON.stringify([objs[9], objs[10], objs[11]]), `[{"v":9},"poly10",{"v":11}]`);
}
