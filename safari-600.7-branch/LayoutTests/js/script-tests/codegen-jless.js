description(
"Tests particular unusual cases of jump-if-less codegen."
);

shouldBe("!(true && undefined > 0) ? 'true' : 'false'", "'true'");
