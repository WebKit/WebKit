//@ requireOptions("--useProbeOSRExit=true", "--forceEagerCompilation=true")

// This test passes if it does not crash especially on ASAN builds.

let x = 0;
function Foo(a) {
    a === a;
    '' + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x + x;
}

for (let i=0; i<8; i++)
    new Foo(0);

new Foo({});
