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

// These must run before anything reifies Date.prototype's static toJSON.
{
    Object.setPrototypeOf(Array.prototype, Date.prototype);
    shouldThrow(() => JSON.stringify([]), "TypeError: Type error");
    shouldThrow(() => JSON.stringify({ a: [] }), "TypeError: Type error");
    Object.setPrototypeOf(Array.prototype, Object.prototype);
    shouldBe(JSON.stringify([]), `[]`);
}

{
    class X { constructor() { this.a = 1; } }
    Object.setPrototypeOf(X.prototype, Date.prototype);
    shouldThrow(() => JSON.stringify(new X), "TypeError: Type error");
    shouldThrow(() => JSON.stringify({ v: new X }), "TypeError: Type error");
    let o = { b: 2 };
    Object.setPrototypeOf(o, Date.prototype);
    shouldThrow(() => JSON.stringify({ o }), "TypeError: Type error");
}

class A { constructor() { this.x = 1; } method() { } get accessor() { return 0; } }
class AA extends A { constructor() { super(); this.y = 2; } }
for (let i = 0; i < 10; ++i) {
    shouldBe(JSON.stringify(new A), `{"x":1}`);
    shouldBe(JSON.stringify({ a: new A, b: [new AA] }), `{"a":{"x":1},"b":[{"x":1,"y":2}]}`);
    shouldBe(JSON.stringify(new AA, null, 1), `{\n "x": 1,\n "y": 2\n}`);
}

{
    class C { constructor() { this.x = 1; } }
    let c = new C;
    for (let i = 0; i < 10; ++i)
        shouldBe(JSON.stringify(c), `{"x":1}`);
    C.prototype.toJSON = function () { return 42; };
    shouldBe(JSON.stringify(c), `42`);
    shouldBe(JSON.stringify({ c }), `{"c":42}`);
    delete C.prototype.toJSON;
    shouldBe(JSON.stringify(c), `{"x":1}`);
    Object.defineProperty(C.prototype, "toJSON", { value() { return "non-enumerable"; }, enumerable: false, configurable: true });
    shouldBe(JSON.stringify(c), `"non-enumerable"`);
}

{
    class D0 { }
    class D1 extends D0 { constructor() { super(); this.y = 2; } }
    let d = new D1;
    for (let i = 0; i < 10; ++i)
        shouldBe(JSON.stringify(d), `{"y":2}`);
    D0.prototype.toJSON = () => "D0";
    shouldBe(JSON.stringify(d), `"D0"`);
}

{
    let count = 0;
    class E { constructor() { this.z = 3; } }
    Object.defineProperty(E.prototype, "toJSON", { get() { count++; return undefined; }, configurable: true });
    for (let i = 0; i < 10; ++i)
        shouldBe(JSON.stringify(new E), `{"z":3}`);
    shouldBe(count, 10);
}

{
    let n = Object.create(null);
    n.k = 1;
    for (let i = 0; i < 10; ++i)
        shouldBe(JSON.stringify({ n }), `{"n":{"k":1}}`);
}

{
    let proto = { inherited: 1 };
    let o = Object.create(proto);
    o.own = 2;
    for (let i = 0; i < 10; ++i)
        shouldBe(JSON.stringify(o), `{"own":2}`);
    proto.toJSON = () => "P";
    shouldBe(JSON.stringify(o), `"P"`);
}

{
    let log = [];
    let p = new Proxy({}, { get(t, k, r) { log.push(String(k)); return Reflect.get(t, k, r); } });
    let q = Object.create(p);
    q.a = 1;
    shouldBe(JSON.stringify(q), `{"a":1}`);
    shouldBe(log.includes("toJSON"), true);
}

{
    class G { constructor() { this.b = Object(1n); } }
    shouldThrow(() => JSON.stringify(new G), "TypeError: JSON.stringify cannot serialize BigInt.");
    shouldBe(JSON.stringify({ s: Object.setPrototypeOf(new String("s"), A.prototype) }), `{"s":"[object String]"}`);
    shouldBe(JSON.stringify({ b: Object.setPrototypeOf(new Boolean(true), A.prototype) }), `{"b":true}`);
}

{
    class F { constructor() { this.w = 4; } }
    let f = new F;
    for (let i = 0; i < 10; ++i)
        shouldBe(JSON.stringify(f), `{"w":4}`);
    Object.prototype.toJSON = function () { return "OP"; };
    shouldBe(JSON.stringify(f), `"OP"`);
    shouldBe(JSON.stringify({}), `"OP"`);
    delete Object.prototype.toJSON;
    shouldBe(JSON.stringify(f), `{"w":4}`);
}
