function opaque()
{
    // This exists to hide side effects to the optimizer.
}
noInline(opaque);

function test(i, opaqueCondition) {
    do {
        if (opaqueCondition == 1) {
            if (i < 42) {
                opaque(i);
                if (i != 41) {
                    break;
                }
            }
        } else if (opaqueCondition == 2) {
            if (i < 42) {
                opaque(i);
                if (i < 41) {
                    opaque(i);
                    if (i == 0) {
                        break;
                    }
                }
            }
        }
    } while (true);

    opaque(i);
    opaque(42);
    opaque(41);
    return i;
}
noInline(test);

function loop() {
    for (let i = 0; i < 1e6; ++i)
        test(1, 1);
}
noInline(loop);
noDFG(loop);

loop();
