//@ skip if $memoryLimited
//@ runDefault("--watchdog=1000", "--watchdog-exception-ok")

function func_1_() {
    var var_3_ = new WebAssembly.Memory({initial: 4096});
    try {
        func_1_();
    } catch {}
    new Promise(function () {
        const v8 = new Int32Array(62013);
        for (const v9 in v8);
    });
}
func_1_();
