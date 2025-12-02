//@ skip if $memoryLimited
function g10(f) {
    function t() {
        try {
            return t();
        } catch (e) {
            return f();
        }
    }
    try {
        return t();
    } catch (e) {}
}

function g8(f) {
    try {
        return f();
    } catch (e) {}
}

function fn1(v2) {
    let v3 = new Array(4);
    v3[0] = 0;
    v3[1] = v3;
    for (let v5 = 2; v5 < v3.length; ++v5) {
        v3[v5] = v3[0];
    }
    if (v2.foo === -0)
        return 0;
    let v4 = g8(() => v3[0]);
    return v3[0] + v3[0] - v3[0];
}

for (let v6 = 0; v6 < testLoopCount; ++v6) {
    let v7 = g10(() => {
        return fn1({
            foo: v6 % 0
        });
    });
}
