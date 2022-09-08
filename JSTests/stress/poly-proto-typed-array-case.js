const typedArray = new Uint8Array();
let key = '0.1';
Object.prototype[key] = undefined;

function bar() {
  function Foo() {}

  for (let i = 0; i < 100; i++) {
    let foo = new Foo();
    typedArray.__proto__ = foo;
    let {x} = foo;
  }
  typedArray[key];
}

for (let i = 0; i < 100; i++) {
  bar();
}
