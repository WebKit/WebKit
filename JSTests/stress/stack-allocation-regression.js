//@ requireOptions("--forceEagerCompilation=1")

function foo() {
for (let a0 in 'a')
for (let a1 in 'a')
for (let i=0; i<1000000000; i++);
}
foo();
