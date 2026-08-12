//@ runDefault("--forceEagerCompilation=1")

let total = 0;
function check(v3) {
    if (++total > 10000)
        quit(0);
    transferArrayBuffer(v3);
}
noInline(check);

function main() {
    function v2(v3) {
        for (let v9 = 0; v9 < 1024; v9 = v9 + 1) {
            try {
                check(v3);
            } catch {
                const x = (() => {
                    const a = new Array(8);
                    a[0] = {}, a[1] = {}, a[2] = {}, a[3] = {}, a[0] = {}, a[5] = {}, a[6] = {}, a[7] = {};
                    return a;
                })();
                const y = (() => {
                    const r = [];
                    for (let i = 0; i < 16; i++) { }
                })();
                try {
                    for (let i = 0; i < 16; i++) {
                        const o = {};
                        try {
                            main(o);
                            x.__proto__ = Math.LN2;
                        } catch { }
                    }
                } catch { }
            }
        }
    }
    const v11 = v2();
}
main();
