//@ skip if $buildType == "debug"
//@ runDefault("--watchdog=4", "--watchdog-exception-ok")

function baz(c) {
  if (c) {
    $vm.haveABadTime();
  }
}
noInline(baz);

function bar() {}

function foo(c, ...args) {
  let args2 = [...args];
  baz(c);
  bar.apply(undefined, args2);
}

for (let i = 0; i < 70000; i++) {
  foo(false, 0);
}
foo(true, 0);
