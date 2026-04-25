description("Test to ensure that global constructors can be deleted");

shouldBeTrue("delete window.CSSRuleList");
shouldBe("window.CSSRuleList", "undefined");

shouldBeTrue("delete window.Document");
shouldBe("window.Document", "undefined");

shouldBeTrue("delete window.Element");
shouldBe("window.Element", "undefined");

shouldBeTrue("delete window.HTMLDivElement");
shouldBe("window.HTMLDivElement", "undefined");

shouldBeTrue("delete window.ProgressEvent");
shouldBe("window.ProgressEvent", "undefined");

shouldBeTrue("delete window.Selection");
shouldBe("window.Selection", "undefined");

shouldBeTrue("delete window.XMLHttpRequest");
shouldBe("window.XMLHttpRequest", "undefined");
