description(
"String.replace(&hellip;) test"
);

var testString = "It's the end of the world as we know it, and I feel fine.";
shouldBe("testString",
         "\"It's the end of the world as we know it, and I feel fine.\"");
shouldBe("testString.replace('end','BEGINNING')",
         "\"It's the BEGINNING of the world as we know it, and I feel fine.\"");
shouldBe("testString.replace(/[aeiou]/gi,'-')",
         "\"-t's th- -nd -f th- w-rld -s w- kn-w -t, -nd - f--l f-n-.\"");
shouldBe("testString.replace(/[aeiou]/gi, function Capitalize(s){ return s.toUpperCase(); })", 
         "\"It's thE End Of thE wOrld As wE knOw It, And I fEEl fInE.\"");
shouldBe("testString.replace(/([aeiou])([a-z])/g, function Capitalize(){ return RegExp.$1.toUpperCase()+RegExp.$2; })",
         "\"It's the End Of the wOrld As we knOw It, And I fEel fIne.\"");
shouldBe("testString.replace(/([aeiou])([a-z])/g, function Capitalize(orig,re1,re2) { return re1.toUpperCase()+re2; })",
        "\"It's the End Of the wOrld As we knOw It, And I fEel fIne.\"");
shouldBe("testString.replace(/(.*)/g, function replaceWithDollars(matchGroup) { return '$1'; })", "\"$1$1\"");
shouldBe("testString.replace(/(.)(.*)/g, function replaceWithMultipleDollars(matchGroup) { return '$1$2'; })", "\"$1$2\"");
shouldBe("testString.replace(/(.)(.*)/, function checkReplacementArguments() { return arguments.length; })", "\"5\"");

var successfullyParsed = true;
