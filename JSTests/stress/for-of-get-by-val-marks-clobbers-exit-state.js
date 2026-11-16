const a0 = new Array(100);
a0.x = 0;

function foo() {
  for (const q of a0) {}
}

for (let i = 0; i < 1000; i++) {
  foo();
}
