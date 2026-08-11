// Hot-path microbenchmark for String.prototype.search with a cached primordial
// RegExp argument. The DFG StringSearch node is converted to RegExpSearch in
// fixup, guarded by a CheckStructure on the primordial RegExp structure.

function search(string, regexp)
{
    return string.search(regexp);
}
noInline(search);

var string = "abc1def2ghi3jkl4mno5";
var regexp = /[0-9]/;
for (var i = 0; i < 1e6; ++i)
    search(string, regexp);
