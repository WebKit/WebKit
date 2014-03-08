description(
"This test makes sure we don't hang we use continue inside a for-of over arguments"
);

function test() {
	var count = 0;
	for (var a of arguments)
		count++;
	return count;
}

shouldBe("test()", "0")
shouldBe("test(1)", "1")
shouldBe("test(1,2)", "2")
shouldBe("test(1,2,3)", "3")
