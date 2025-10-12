//@ $skipModes << :lockdown if $buildType == "debug"

function bar(...a) {
  return a;
}

function foo(...a) {
  let x = bar([]);
  let [] = a;
  return bar(...x);
}

for (let i = 0; i < 10000000; i++) {
  foo();
}
