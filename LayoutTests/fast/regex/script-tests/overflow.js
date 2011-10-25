description("This test checks expressions with alternative lengths of appox. 2^31.");

var regexp1 = /(?:(?=g))|(?:m).{2147483648,}/;
shouldBe("regexp1.exec('')", 'null');

var regexp2 = /(?:(?=g)).{2147483648,}/;
shouldBe("regexp2.exec('')", 'null');

