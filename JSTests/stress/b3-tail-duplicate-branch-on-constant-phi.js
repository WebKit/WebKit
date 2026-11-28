function A(v) { this.a = v; }
A.prototype.apply = function (x) { return (x + this.a) | 0; };
function B(v) { this.b = v; }
B.prototype.apply = function (x) { return (x * this.b) | 0; };
function C(v) { this.c = v; }
C.prototype.apply = function (x) { return (x - this.c) | 0; };
function D(v) { this.d = v; }
D.prototype.apply = function (x) { return x ^ this.d; };

function dispatch(objects, count) {
    let result = 0;
    for (let i = 0; i < count; ++i)
        result = objects[i & 3].apply(result + i);
    return result;
}
noInline(dispatch);

function expectedDispatch(count, replaced) {
    let result = 0;
    for (let i = 0; i < count; ++i) {
        let x = result + i;
        switch (i & 3) {
        case 0: result = (x + 1) | 0; break;
        case 1: result = (x * 3) | 0; break;
        case 2: result = replaced ? (x + 100) | 0 : (x - 5) | 0; break;
        case 3: result = x ^ 7; break;
        }
    }
    return result;
}
noInline(expectedDispatch);

let objects = [new A(1), new B(3), new C(5), new D(7)];
for (let i = 0; i < testLoopCount; ++i) {
    let count = 16 + (i & 15);
    let actual = dispatch(objects, count);
    let expected = expectedDispatch(count, false);
    if (actual !== expected)
        throw new Error("dispatch(" + count + "): expected " + expected + " but got " + actual);
}

C.prototype.apply = function (x) { return (x + 100) | 0; };
for (let i = 0; i < testLoopCount; ++i) {
    let count = 16 + (i & 15);
    let actual = dispatch(objects, count);
    let expected = expectedDispatch(count, true);
    if (actual !== expected)
        throw new Error("dispatch after replacement(" + count + "): expected " + expected + " but got " + actual);
}
