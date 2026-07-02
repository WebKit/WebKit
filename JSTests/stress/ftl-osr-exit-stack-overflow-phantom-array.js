//@ runDefault("--watchdog=1000", "--watchdog-exception-ok")
function opt() {
    const arr = Array(10);
    arr[0] = 0;
    function foo() { return arr; }
    try {
        opt();
        opt();
    } catch (e) {
    }
}

for (let i = 0; i < 100000; i++)
    opt();
