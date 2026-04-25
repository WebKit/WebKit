let a = {};
let b = $.createRealm();
a.__proto__ = b;

for (let i = 0; i < 1000; i++) {
  a.IsHTMLDDA = undefined;
}
