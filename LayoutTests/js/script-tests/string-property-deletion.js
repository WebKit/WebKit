description("This page tests deletion of properties on a string object.");

var str = "abc";
shouldBe('str.length', '3');
shouldBe('delete str.length', 'false');
shouldBe('delete str[0]', 'false');
shouldBe('delete str[1]', 'false');
shouldBe('delete str[2]', 'false');
shouldBe('delete str[3]', 'true');
shouldBe('delete str[-1]', 'true');
shouldBe('delete str[4294967294]', 'true');
shouldBe('delete str[4294967295]', 'true');
shouldBe('delete str[4294967296]', 'true');
shouldBe('delete str[0.0]', 'false');
shouldBe('delete str[0.1]', 'true');
shouldBe('delete str[\'0.0\']', 'true');
shouldBe('delete str.foo', 'true');
