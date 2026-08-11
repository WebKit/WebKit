importScripts('../../../resources/js-test-pre.js');

var global = this;
global.jsTestIsAsync = true;

description("tests that atob() / btoa() work in workers.");

shouldBe('self.atob("YQ==")', '"a"');
shouldBe('self.atob("YWI=")', '"ab"');
shouldBe('self.atob("YWJj")', '"abc"');
shouldBe('self.atob("YWJjZA==")', '"abcd"');
shouldBe('self.atob("YWJjZGU=")', '"abcde"');
shouldBe('self.atob("YWJjZGVm")', '"abcdef"');
shouldBe('self.btoa("a")', '"YQ=="');
shouldBe('self.btoa("ab")', '"YWI="');
shouldBe('self.btoa("abc")', '"YWJj"');
shouldBe('self.btoa("abcd")', '"YWJjZA=="');
shouldBe('self.btoa("abcde")', '"YWJjZGU="');
shouldBe('self.btoa("abcdef")', '"YWJjZGVm"');

shouldBe('typeof self.btoa', '"function"');
shouldThrow('self.btoa()', '"TypeError: Not enough arguments"');
shouldBe('self.btoa("")', '""');
shouldBe('self.btoa(null)', '"bnVsbA=="'); // Gets converted to "null" string.
shouldBe('self.btoa(undefined)', '"dW5kZWZpbmVk"');
shouldBe('self.btoa(self)', '"W29iamVjdCBEZWRpY2F0ZWRXb3JrZXJHbG9iYWxTY29wZV0="'); // "[object DedicatedWorkerGlobalScope]"
shouldBe('self.btoa("éé")', '"6ek="');
shouldBe('self.btoa("\\u0080\\u0081")', '"gIE="');
shouldThrow('self.btoa("тест")');
self.btoa = 0;
shouldBe('self.btoa', '0');
shouldBe('typeof self.btoa', '"number"');

shouldBe('typeof self.atob', '"function"');
shouldThrow('self.atob()', '"TypeError: Not enough arguments"');
shouldBe('self.atob("")', '""');
shouldBe('self.atob(null)', '"\x9Eée"'); // Gets converted to "null" string.
shouldThrow('self.atob(undefined)');
shouldBe('self.atob(" YQ==")', '"a"');
shouldBe('self.atob("YQ==\\u000a")', '"a"');
shouldBe('self.atob("ab\\tcd")', '"i·\x1d"');
shouldBe('self.atob("ab\\ncd")', '"i·\x1d"');
shouldBe('self.atob("ab\\fcd")', '"i·\x1d"');
shouldBe('self.atob("ab cd")', '"i·\x1d"');
shouldBe('self.atob("ab\\t\\n\\f\\r cd")', '"i·\x1d"');
shouldBe('self.atob(" \\t\\n\\f\\r ab\\t\\n\\f\\r cd\\t\\n\\f\\r ")', '"i·\x1d"');
shouldBe('self.atob("ab\\t\\n\\f\\r =\\t\\n\\f\\r =\\t\\n\\f\\r ")', '"i"');
shouldBe('self.atob("            ")', '""');
shouldThrow('self.atob(" abcd===")');
shouldThrow('self.atob("abcd=== ")');
shouldThrow('self.atob("abcd ===")');
shouldBe('self.atob("6ek=")', '"éé"');
shouldBe('self.atob("6ek")', '"éé"');
shouldBe('self.atob("gIE=")', '"\u0080\u0081"');
shouldThrow('self.atob("тест")');
shouldThrow('self.atob("z")');
shouldBe('self.atob("zz")', '"Ï"');
shouldBe('self.atob("zzz")', '"Ï\u003C"');
shouldBe('self.atob("zzz=")', '"Ï\u003C"');
shouldThrow('self.atob("zzz==")'); // excess pad characters.
shouldThrow('self.atob("zzz===")'); // excess pad characters.
shouldThrow('self.atob("zzz====")'); // excess pad characters.
shouldThrow('self.atob("zzz=====")'); // excess pad characters.
shouldBe('self.atob("zzzz")', '"Ï\u003Có"');
shouldThrow('self.atob("zzzzz")');
shouldThrow('self.atob("z=zz")');
shouldThrow('self.atob("=")');
shouldThrow('self.atob("==")');
shouldThrow('self.atob("===")');
shouldThrow('self.atob("====")');
shouldThrow('self.atob("=====")');
self.atob = 0;
shouldBe('self.atob', '0');
shouldBe('typeof self.atob', '"number"');

finishJSTest();

