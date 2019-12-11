//@ requireOptions("--jitPolicyScale=0")

// This test passes if it does not crash.

let x = {}
let enUS = ['en', 'US'].join('-')
for (let i=0; i<100; i++) {
    Intl.NumberFormat(enUS)
}
for (let i=0; i<10000; i++) {
    x[enUS]
};
