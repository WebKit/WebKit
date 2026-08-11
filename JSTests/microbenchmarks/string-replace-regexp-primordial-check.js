// String.prototype.replace with a cached primordial RegExp argument and a no-match
// pattern, so the per-iteration cost is dominated by the primordial-property checks
// that fixup inserts in front of StringReplace(RegExp).

function replace(string, regexp, replacement)
{
    return string.replace(regexp, replacement);
}
noInline(replace);

var string = "abc";
var regexp = /x/;
for (var i = 0; i < 1e7; ++i)
    replace(string, regexp, "y");
