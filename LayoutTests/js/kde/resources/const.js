// constant definition
const c = 11;
if (c !== 11)
    testFailed("c is not eleven");
else
    testPassed("c is eleven");

// attempt to redefine should have no effect
var threwError = false;
try {
    c = 22;
} catch(e) {
    threwError = true;
}
if (threwError)
    testPassed("cant assign to const");
else
    testFailed("const assignment worked but should not have");

if (c !== 11)
    testFailed("c is not eleven");
else
    testPassed("c is eleven");

const dummy = 0;
for (var v = 0;;) {
  ++v;
  shouldBe("v", "1");
  break;
}

// ### check for forbidden redeclaration
