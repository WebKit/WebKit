// The regression test checks that multiple non-greedy character classes backtrack properly.

let re = /[^\/]+\/xxx\/[^\/]+?\/[^\/]+?\/[^\/]+?/;
let str;
let match;

str = "blah/xxx/blah/blah_blah_blah_blah_blah_blah_blah_blah_blah_blah/";
match = re.exec(str);
if (match !== null)
    throw(re + ".exec(\"" + str + "\") Should not have matched!");

str = "blah/xxx/blah/blah_blah_blah_blah/";
match = re.exec(str);
if (match !== null)
    throw(re + ".exec(\"" + str + "\") Should not have matched!");
