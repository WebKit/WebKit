function foo() {
  for (let i=0; i<testLoopCount; i++) {}
  for (const q of Array.prototype) {}
}

for (let i=0; i<1000; i++) {
  foo();
}
