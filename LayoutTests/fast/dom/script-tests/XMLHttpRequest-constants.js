description("This test checks the constants on the XMLHttpRequst object, as well as its prototype and constructor.");

debug("Test the constants on the XMLHttpRequest object itself.")
shouldBe("new XMLHttpRequest().UNSENT", "0");
shouldBe("new XMLHttpRequest().OPENED", "1");
shouldBe("new XMLHttpRequest().HEADERS_RECEIVED", "2");
shouldBe("new XMLHttpRequest().LOADING", "3");
shouldBe("new XMLHttpRequest().DONE", "4");

debug("Test the constants on the XMLHttpRequest prototype object.")
shouldBe("XMLHttpRequest.prototype.UNSENT", "0");
shouldBe("XMLHttpRequest.prototype.OPENED", "1");
shouldBe("XMLHttpRequest.prototype.HEADERS_RECEIVED", "2");
shouldBe("XMLHttpRequest.prototype.LOADING", "3");
shouldBe("XMLHttpRequest.prototype.DONE", "4");

debug("Test the constants on the XMLHttpRequest constructor object.")
shouldBe("XMLHttpRequest.UNSENT", "0");
shouldBe("XMLHttpRequest.OPENED", "1");
shouldBe("XMLHttpRequest.HEADERS_RECEIVED", "2");
shouldBe("XMLHttpRequest.LOADING", "3");
shouldBe("XMLHttpRequest.DONE", "4");
