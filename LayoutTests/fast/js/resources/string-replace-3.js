description(
"String.replace(&hellip;) test &mdash; $&amp;, $`, $' and $nn"
);

var testString = "It's the end of the world as we know it, and I feel fine.";
shouldBe("testString",
         "\"It's the end of the world as we know it, and I feel fine.\"");
shouldBe("testString.replace(/I feel/, 'yet $& just')",
         "\"It's the end of the world as we know it, and yet I feel just fine.\"");
shouldBe("testString.replace(/the end/, 'nice. $`a picture')",
         "\"It's nice. It's a picture of the world as we know it, and I feel fine.\"");
shouldBe('testString.replace(/feel fin/, "am gon$\' By")',
         "\"It's the end of the world as we know it, and I am gone. Bye.\"");
shouldBe("testString.replace(/(t)(h)(e e)(n)(d)( o)(f)( )(t)(h)(e )([^r]*)(rld)/, 'not $12olly mammoth')",
         "\"It's not woolly mammoth as we know it, and I feel fine.\"");

shouldBe("testString.replace('I feel', 'yet $& just')",
         "\"It's the end of the world as we know it, and yet I feel just fine.\"");
shouldBe("testString.replace('the end', 'nice. $`a picture')",
         "\"It's nice. It's a picture of the world as we know it, and I feel fine.\"");
shouldBe('testString.replace("feel fin", "am gon$\' By")',
         "\"It's the end of the world as we know it, and I am gone. Bye.\"");

shouldBe("testString.replace(/end/, '$$ $0 $00 $1 $01 $2 $12 $9 $99 $1a $11a')",
         "\"It's the $ $0 $00 $1 $01 $2 $12 $9 $99 $1a $11a of the world as we know it, and I feel fine.\"");
shouldBe("testString.replace('end', '$$ $0 $00 $1 $01 $2 $12 $9 $99 $1a $11a')",
         "\"It's the $ $0 $00 $1 $01 $2 $12 $9 $99 $1a $11a of the world as we know it, and I feel fine.\"");

shouldBe("testString.replace(/(e)(n)(d) (o)(f) (t)(h)(e) (w)(o)(r)(l)(d)/, '$$ $0 $00 $1 $01 $2 $12 $9 $99 $1a $11a')",
         "\"It's the $ $0 $00 e e n l w w9 ea ra as we know it, and I feel fine.\"");

var successfullyParsed = true;
