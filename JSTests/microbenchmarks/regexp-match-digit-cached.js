// Hot-path microbenchmark for String.prototype.match with a cached primordial
// RegExp argument. The DFG StringMatch node is converted to RegExpMatchFast in
// fixup, guarded by a CheckStructure on the primordial RegExp structure.

function match(string, regexp)
{
    return string.match(regexp);
}
noInline(match);

var string = "abc1def2ghi3jkl4mno5";
var regexp = /[0-9]/g;
for (var i = 0; i < 1e6; ++i)
    match(string, regexp);
