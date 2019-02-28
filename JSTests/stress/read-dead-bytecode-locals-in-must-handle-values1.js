//@ runDefault("--useMaximalFlushInsertionPhase=1", "--useConcurrentJIT=0")
function bar(x) {
  if (x) {
    return;
  }
  x = x ** 0;
  x = x * 2;
}

function foo() {
  bar();
  for (let i = 0; i < 10; ++i) {
  }
}

for (var i = 0; i < 10000; ++i) {
  foo();
}
