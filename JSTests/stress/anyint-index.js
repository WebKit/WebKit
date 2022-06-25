let ta = new Uint8Array(1);

function foo(arg0) {
  'a'.__defineGetter__('x', () => {
    arg0;
  });
  arg0 **= 0;
  ta[arg0];
}


for (let i = 0; i < 10000; i++) {
  foo(0);
}
