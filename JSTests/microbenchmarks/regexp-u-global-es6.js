//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function fn() {
    return '𠮷'.match(/^.$/ug);
}
noInline(fn);

for (var i = 0; i < 1e6; ++i)
    fn();
