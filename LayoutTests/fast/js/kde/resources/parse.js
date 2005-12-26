function test() { return;}
function test() { while(0) break; }
function test() { while(0) continue; }

function test() { return lab;}
function test() { while(0) break lab; }
function test() { while(0) continue lab; }

function test() { return }
function test() { while(0) break }
function test() { while(0) continue }

function test() { return 0 }
function test() { while(0) break lab }
function test() { while(0) continue lab }

lab:

a = 1
b = 123 // comment
c = 2
c = 3 /* comment */
d = 4

// non-ascii identifier letters (not working copy of Mozilla?!)
var ident = "";
ident += "\u00E9"; // LATIN SMALL LETTER E WITH ACUTE
ident += "\u0100"; // LATIN CAPITAL LETTER A WITH MACRON
ident += "\u02af"; // LATIN SMALL LETTER TURNED H WITH FISHHOOK AND TAIL
ident += "\u0388"; // GREEK CAPITAL LETTER EPSILON WITH TONOS
ident += "\u18A8"; // MONGOLIAN LETTER MANCHU ALI GALI BHA
var code = "var " + ident + " = 11; " + ident + ";";
var res = eval(code);
shouldBe("res", "11");

// invalid identifier letter
var caught = false;
try {
  eval("var f\xf7;"); // 
} catch (e) {
  caught = true;
}
shouldBeTrue("caught");

successfullyParsed = true
