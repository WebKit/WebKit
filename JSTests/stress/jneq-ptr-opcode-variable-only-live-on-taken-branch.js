function bar() {}

function foo() {
  let x = 0;
  try {
    undefined instanceof {};
  } catch {}
  bar.apply(0, [], x);
}
noInline(foo);

for (let i=0; i < 10000; i++) {
  foo();
}
