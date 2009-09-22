description(
'Tests some regular expressions that were doing the wrong thing with the "find first asserted" optimization.'
);

shouldBe('/.*<body>(.*)<\\/body>.*/.exec("foo<body>bar</body>baz").toString()', '"foo<body>bar</body>baz,bar"');
shouldBe('/\\s*<!--([\s\S]*)\\/\\/\\s*-->\\s*/.exec("<!--// -->").toString()', '"<!--// -->,"');

debug('');

var successfullyParsed = true;
