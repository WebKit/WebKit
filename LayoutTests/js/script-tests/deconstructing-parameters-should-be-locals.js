description("This tests to ensure that ddeconstructing parameters behave like regular locals")

var value="outer"
function readDeconstructedParameter([value]) {
    return value;
}

function overwriteDeconstructedParameter([value]) {
	value = "inner"
}

function readCapturedDeconstructedParameter([value]) {
	return (function () {
	    return value;
	})()
}

function overwriteCapturedDeconstructedParameter([value]) {
	(function () {
	    value = "innermost";
	})()
	return value
}

shouldBe("readDeconstructedParameter(['inner'])", "'inner'")
overwriteDeconstructedParameter(['inner'])

shouldBe("overwriteDeconstructedParameter(['unused']); value;", "'outer'")

shouldBe("readCapturedDeconstructedParameter(['inner'])", "'inner'")
overwriteDeconstructedParameter(['inner'])

shouldBe("overwriteCapturedDeconstructedParameter(['unused']);", "'innermost'")
shouldBe("value", "'outer'")

