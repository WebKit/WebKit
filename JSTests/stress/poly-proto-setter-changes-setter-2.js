//@ skip if not $jitTests
//@ requireOptions("--forcePolyProto=1", "--useLLInt=0", "--repatchBufferingCountdown=0")

function assert_eq(a, b) {
    
}
noInline(assert_eq)

class A {
    set x(v) {
        if (v == 1) {
            Object.defineProperty(A.prototype, "x", {
                get: function() {
                    return 42
                },
                set: undefined
            });
        }
        if (v > 1)
            throw new Error()
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
    assert_eq(a.x, i == 0 ? undefined : 42)
}


