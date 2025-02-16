function foo(a0, a1, a2, a3, a4, a5, a6, a7, a8) {
  return arguments;
}

function bar() {
  let args = foo(0, 0);
  baz.apply(null, args);
}

function baz() {}

for (let i = 0; i < testLoopCount; ++i) {
  bar();
}
