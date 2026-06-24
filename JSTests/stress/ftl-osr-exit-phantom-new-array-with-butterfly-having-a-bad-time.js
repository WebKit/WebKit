//@ runDefault("--jitPolicyScale=0.1")

let trigger = false;

function cb() {
    if (trigger) {
        Object.defineProperty(Array.prototype, 0, {
            get() { return 42; }, configurable: true
        });
    }
}
noInline(cb);

function collect() { gc(); }
noInline(collect);

function opt() {
    let a = new Array(5);
    a[0] = 1.1;
    a[1] = 2.2;
    a[2] = 3.3;
    a[3] = 4.4;
    a[4] = 5.5;
    cb();
    collect();
    return a[0] + a[1] + a[2] + a[3] + a[4];
}
noInline(opt);

for (let i = 0; i < 1000; i++)
    opt();

trigger = true;
opt();
gc();
