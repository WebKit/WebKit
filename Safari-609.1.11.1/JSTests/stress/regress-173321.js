var checks = 0;

function shouldBe(o, testObj)
{
    checks = checks + 1;

    if (o.a != testObj.a)
        throw "Check #" + checks + " o.a should be " + testObj.a + " , is " + o.a;

    if (o.b != testObj.b)
        throw "Check #" + checks + " o.b should be " + testObj.b + " , is " + o.b;

    if (o.c != testObj.c)
        throw "Check #" + checks + " o.c should be " + testObj.c + " , is " + o.c;

    if (o.p != testObj.p)
        throw "Check #" + checks + " o.p should be " + testObj.p + " , is " + o.p;

    if (o.x != testObj.x)
        throw "Check #" + checks + " o.x should be " + testObj.x + " , is " + o.x;

    if (o.y != testObj.y)
        throw "Check #" + checks + " o.y should be " + testObj.y + " , is " + o.y;
}

var testObjInitial = { a: 0, b: 1, c: 2, p: 100, x: 10, y: 11 };
var testObjAfterReadOnlyProperty = { a: 101, b: 1, c: 2, p: 100, x: 10, y: 11 };

var SimpleObject = function () {
    this.a = 0;
    this.b = 1;
    this.c = 2;
}

var proto = { p: 100 };

SimpleObject.prototype = proto;

var test = function () {
    var o = new SimpleObject();
    o.x = 10;
    o.y = 11;
    return o;
}

shouldBe(test(), testObjInitial);
shouldBe(test(), testObjInitial);
shouldBe(test(), testObjInitial);

// Change the prototype chain by making "a" read-only.
Object.defineProperty(proto, "a", { value: 101, writable: false });

// Run a bunch of times to tier up.
for (var i = 0; i < 10000; i++)
    shouldBe(test(), testObjAfterReadOnlyProperty);

