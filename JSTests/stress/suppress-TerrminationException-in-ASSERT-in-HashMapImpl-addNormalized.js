//@ runDefault("--watchdog=300", "--watchdog-exception-ok")

function foo() {
  let s = new Set();
  gc();
  s.add('1'); 
}

for (let i=0; i<1000; i++) {
  foo();
}
