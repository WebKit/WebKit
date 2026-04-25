function getter() {
  if (+undefined) {
    return '';
  }
}

function foo() {}

Object.defineProperty(foo, 'name', {get: getter});

for (let i=0; i<testLoopCount; i++) {
  foo.bind(null, foo, null);
}
