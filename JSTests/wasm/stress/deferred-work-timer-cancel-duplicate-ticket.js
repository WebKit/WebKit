var N_REGISTRIES = 300;
var N_DEFERRED_WORK1 = 400;
var N_DEFERRED_WORK2 = 1500;

var sab = new SharedArrayBuffer(8);
var i32 = new Int32Array(sab);

function setupChildGlobal() {
    var childGlobal = createGlobalObject();
    childGlobal.N_REGISTRIES = N_REGISTRIES;
    childGlobal.eval(
        'globalThis.__registries = [];' +
        'for (var k = 0; k < this.N_REGISTRIES; k++) {' +
        '    var fr = new FinalizationRegistry(function(h){});' +
        '    (function(){ fr.register({}, 1); })();' +
        '    globalThis.__registries.push(fr);' +
        '}'
    );
    gc();
    return childGlobal;
}
noDFG(setupChildGlobal);

function run() {
    var childGlobal = setupChildGlobal();
    globalThis.p1 = Atomics.waitAsync(i32, 0, 0).value;
    Atomics.notify(i32, 0);
    childGlobal = null;
    return 0;
}
noDFG(run);
run();

p1.then(function () {
    for (var i = 0; i < N_DEFERRED_WORK1; i++)
        setTimeout(function () { }, 10);
});

gc(); gc(); gc();

var p2 = Atomics.waitAsync(i32, 0, 0).value;
Atomics.notify(i32, 0);
p2.then(function () {
    for (var j = 0; j < N_DEFERRED_WORK2; j++)
        setTimeout(function () { }, 50);

    gc(); gc();
    globalThis.__fns = [];
    for (var k = 0; k < 10; k++) __fns.push(function () { });
});

setTimeout(function () { }, 200);
