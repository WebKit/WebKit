let p = [0, 0];
Object.defineProperty(p, '0', {});
function foo() {}
foo.prototype = p;
let x = new foo();
x[0] = 0;
x.reverse();
Object.defineProperty(x, '1', {});
