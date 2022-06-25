description("Regresion test for 158080. This test should pass and not crash.");

shouldThrow("let r = /\\u{|abc/u");
shouldThrow("let r = /\\u{/u");
shouldThrow("let r = /\\u{1/u");
shouldThrow("let r = /\\u{12/u");
shouldThrow("let r = /\\u{123/u");
shouldThrow("let r = /\\u{1234/u");
shouldThrow("let r = /\\u{abcde/u");
shouldThrow("let r = /\\u{abcdef/u");
shouldThrow("let r = /\\u{1111111}/u");
shouldThrow("let r = /\\u{fedbca98}/u");
shouldThrow("let r = /\\u{1{123}}/u");
