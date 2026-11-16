function bar() {
  for (let i=0; i<100; i++) {
    arguments[1];
  }
}

function foo() {
  for (let i=0; i<testLoopCount; i++) {
    bar(...'aa');
  }
}

foo();
