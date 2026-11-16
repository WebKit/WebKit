function foo(a0) {
  Number.prototype.__proto__ = [];
  for (let i = 0; i < testLoopCount; i++) {}
  for (let q of a0) {}
}

foo([]);
foo(0);