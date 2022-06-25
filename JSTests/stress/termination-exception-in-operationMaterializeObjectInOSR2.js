//@ skip if $buildType == "release"
//@ runDefault("--watchdog=90", "--jitPolicyScale=0", "--watchdog-exception-ok")

function bar() {
  try {
    undefined instanceof [];
  } catch {}
  let a = [0];
  for (let i = 0; i < 100; i++) {}
  foo();
  let b = a;
}

function foo() {
  bar();
}

bar();
