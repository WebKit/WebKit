// Regression test for the DFG VirtualRegisterAllocationPhase crash that
// happened when the fast Array iterator entries path emitted GetByVal on a
// Double-storage array followed by NewArray with no intervening ExitOK,
// causing Fixup to insert the ValueRep conversion before the GetByVal that
// produced its operand.

function inner(iterator) {
    for (const item of iterator) { }
}

function driver() {
    const values = [268435456, 4294967295, 268435456, 268435456, 268435456];
    const iterator = values.entries();
    for (let i = 0; i < 100; ++i)
        inner(iterator);
}

for (let i = 0; i < testLoopCount; ++i)
    driver();
