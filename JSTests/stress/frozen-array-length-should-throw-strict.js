const a = Object.freeze(['a']);

function test(a) {
    "use strict";

    try {
        a.length = 2;
    } catch (e) {
        if (e instanceof TypeError)
            return;
    }
    throw new Error("didn't throw the right exception");
}
noInline(test);

for (let i = 0; i < 10000; i++)
    test(a);
