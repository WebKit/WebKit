function runNearStackLimit(f) {
    try {
        return t();
    } catch (e) {
        return f();
    }
}
let flag = false;
function f1() {
    return flag ? {} : 10;
}
noInline(f1);

function f2() {
}

function f3(arg) {
    let r = -(arg ? f1() : f2());
}

for (let i = 0; i < 100000; ++i) {
    try {
        f3(!!(i % 2));
    } catch (e) {}
} 

flag = true;
for (let i = 0; i < 100000; ++i) try {
    runNearStackLimit(() => {
        return f3(!!(i % 2));
    });
} catch (e) {}
