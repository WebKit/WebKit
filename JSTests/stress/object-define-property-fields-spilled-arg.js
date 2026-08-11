// Regression test for a DFG/FTL frame layout bug. operationObjectDefinePropertyFromFields
// used to take 9 GPR args, exceeding ARM64's 8-arg-register budget (and x86_64's 6).
// The 9th argument was poked to [sp + 0], but maxFrameExtentForSlowPathCall is 0 on
// those targets, so [sp + 0] aliased the lowest spill slot and corrupted whatever was
// spilled there. Here, the spilled value happens to be the function argument that
// later feeds a ValueAdd; reading the corrupted slot crashed in operationValueAddNotNumber.
// The fix passes the six descriptor slots through a scratch buffer instead of as
// direct C-call arguments, keeping the operation at four GPR args.

function opt(input) {
    Object.defineProperty((function (t, x) { t.y = x; }), 'reject', { get: (({ valueOf: (/(?<!x)y/.test(input)), c: -5.3049894784e-314, this: 1_000_000 }).prototype &&= "ab") });
    a2 = ["ab"];
    try {
        let combined = a2 + input;
        class Inner {
            method() { return combined; }
            delete = ((unused) => this.species)();
        }
    } catch (x) { }
}
for (let i = 0; i < testLoopCount; i++) {
    try {
        opt(-5.3049894784e-314);
    } catch (e) { }
}
