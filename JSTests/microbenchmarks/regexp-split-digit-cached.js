// Hot-path microbenchmark for String.prototype.split with a primordial
// RegExp separator. This exercises the DFG direct-call inlining path that
// emits a Call to RegExp.prototype[@@split] guarded by CheckStructure.

function split(string, regexp)
{
    return string.split(regexp);
}
noInline(split);

var string = "abc1def2ghi3jkl4mno5";
var regexp = /[0-9]/;
for (var i = 0; i < 1e6; ++i)
    split(string, regexp);
