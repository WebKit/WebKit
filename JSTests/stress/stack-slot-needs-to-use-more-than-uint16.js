//@ skip if $buildType == "debug"

let script = '_,'.repeat(5000);
script += '';
let g = new Function(script, 'if (0) g();');
for (let i = 0; i < 1000; ++i) {
  g(0);
}
