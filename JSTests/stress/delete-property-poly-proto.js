//@ skip if not $jitTests
//@ requireOptions("--forcePolyProto=1", "--useLLInt=0")

class A {
    set x(v) {
        if (v === 1) {
            delete A.prototype.x;
        }
    }

    get y() {
        if (this._y === 1) {
            delete A.prototype.y;
        }
        this._y++;
    }

    set z(v) {
        if (v === 1) {
            delete A.prototype.z;
        }
    }
}

class B extends A {}

let a = new B();
for (let i = 0; i < 10; i++) {
    a.x = i;
}

for (let i = 0; i < 10; i++) {
    a["z"] = i;
}

a._y = 0;
for (let i = 0; i < 15; i++) {
    a.y;
}

if (a._y != 2)
    throw new Error()
