description("HTMLIsIndexElement should inherit directly from HTMLElement and be an HTMLUnknownElement");

var isIndex = document.createElement("isindex");

shouldBe("isIndex.__proto__", "HTMLUnknownElement.prototype");
shouldBe("isIndex.__proto__.__proto__", "HTMLElement.prototype");

shouldBeUndefined("isIndex.prompt");
shouldBeUndefined("isIndex.form");

shouldBeUndefined("isIndex.defaultValue");
shouldBeUndefined("isIndex.disabled");
shouldBeUndefined("isIndex.multiple");
shouldBeUndefined("isIndex.alt");
shouldBeUndefined("isIndex.accept");
