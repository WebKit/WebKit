description("This test checks expressions with alternative lengths of appox. 2^31.");

var regexp1 = /(?:(?=g))|(?:m).{2147483648,}/;
shouldBe("regexp1.exec('')", 'null');

var regexp2 = /(?:(?=g)).{2147483648,}/;
shouldBe("regexp2.exec('')", 'null');

var s3 = "&{6}u4a64YfQP{C}u88c4u5772Qu8693{4294967167}u85f2u7f3fs((uf202){4})u5bc6u1947";
var regexp3 = new RegExp(s3, "");
shouldBe("regexp3.exec(s3)", 'null');

shouldThrow("function f() { /[^a$]{4294967295}/ }", '"SyntaxError: Invalid regular expression: number too large in {} quantifier"');
