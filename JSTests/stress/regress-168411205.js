//@ runDefault("--useConcurrentJIT=0", "--validateGraphAtEachPhase=1", "--validateGraph=true")

function f() {
  const a = new Array(3);
  a[0] = 0;
  for (let i = 1; i < 3; ++i) {
    a[i] = a[0];
  }
}
noInline(f);

for (let i = 0; i < testLoopCount; ++i) {
  f();
}
