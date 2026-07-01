//@ skip if $memoryLimited
//@ runDefault

const PAD = " ".repeat(5000000);
const src =
  'function* gen(){var b;0+' + PAD + '(b,yield,1);null.x;} this.gen=gen;';
(0, eval)(src);

const it = gen();
it.next();

var stackStr;
try {
    it.next();
} catch (e) {
    stackStr = e.stack + 1;
}
