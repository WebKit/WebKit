// constant definition
const c = 11;
shouldBe("c", "11");

// attempt to redefine should have no effect
c = 22;
shouldBe("c", "11");

const dummy = 0;
for (var v = 0;;) {
  ++v;
  shouldBe("v", "1");
  break;
}

// ### check for forbidden redeclaration
