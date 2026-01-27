//@ requireOptions("--validateFTLOSRExitLiveness=true")

function F(a2, ...a3) {
    if (!new.target) { throw 'must be called with new'; }
}
class C extends F {
}
noInline(C);

for (let i = 0; i < testLoopCount; ++i) {
    new C(1, 2, 5, 3);
    new C(1, 2);
    new C();
}

function F2(a, ...a2) {
    if (!a)
        throw a2;
}
class C2 extends F2 { }
noInline(C2);

for (let i = 0; i < testLoopCount; ++i) {
    try {
        new C2(0, 2, 5, 3);
    } catch { }
    new C2(1, 2);
    try {
        new C2();
    } catch { }
}