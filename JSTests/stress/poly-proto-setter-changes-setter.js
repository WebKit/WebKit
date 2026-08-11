//@ skip if not $jitTests
//@ requireOptions("--forcePolyProto=1", "--useLLInt=0", "--repatchBufferingCountdown=0")

function assert_eq(a, b) {
    if (a !== b)
        throw new Error("assertion failed: " + a + " === " + b);
}
noInline(assert_eq)

class A {
    set x(v) {
        if (v == 1) {
            Object.defineProperty(A.prototype, "x", {
                value: 42,
                writable: true
            });
        }
    }
}

class B extends A {
    set y(v) {}
    set y1(v) {}
    set y2(v) {}
}

class C extends B {

}

let a = new C();
for (let i = 0; i < 30; i++) {
    a.x = i;
    assert_eq(a.x, i == 0 ? undefined : i == 1 ? 42 : i);
}

