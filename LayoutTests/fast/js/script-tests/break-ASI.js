description("Test to make sure we don't incorrectly insert a semi colon after a break statement");

shouldBeTrue("do { if(0) break\n;else true; } while (0)")
shouldBeTrue("do { if(0) continue\n;else true; } while (0)")
shouldBeTrue("(function(){if (0) return\n;else return true;})()")
shouldBeTrue("do { if(0) throw 'x';else true; } while (0)")
shouldThrow("if (0) throw\n'Shouldn\'t have parsed this.';")
