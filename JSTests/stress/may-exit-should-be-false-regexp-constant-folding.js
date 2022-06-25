//@ runDefault("--jitPolicyScale=0", "--useConcurrentJIT=0", "--validateGraphAtEachPhase=1")

let re0 = /a/;
let str0 = 'b';
function foo() {
  /a/.exec('b');
  for (var i = 0; i < 6; i++) {
  }
  for (var i = 0; i < 3; i++) {
    re0.exec('a');
  }
  str0.match(/a/);
  for (var i = 0; i < 2; i++) {
    str0.match(/a/g);
  }
}
function bar() {
  for (var i = 0; i < 6; i++) {
    'a'.match(/b/);
  }
}

foo();
bar();
foo();
