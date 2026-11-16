function foo() {}
function bar(a0, a1) {
  a0 instanceof foo;
  a0 === a1;
}
for (let i=0; i<testLoopCount; i++) {
  bar({});
  bar(1n, '');
}
