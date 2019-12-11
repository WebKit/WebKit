//@ runDefault("--forceEagerCompilation=true")

// This test should not crash.

[0, 1].forEach(()=>{
    [{}, 1, 2].forEach(x => {
        ['xy'].indexOf('xy_'.substring(0, 2));
    });
});
