description(
"Tests that our parse-time constant folding treats !(0/0) correctly."
);

shouldBe("!(0/0)", "true");
