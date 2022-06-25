description(
"This will test string.replace with function replacer."
);

shouldBe('"ABC".replace("B","$$")', '"A$C"');
shouldBe('"ABC".replace("B", function () { return "$$"; })', '"A$$C"');
shouldBe('"ABC".replace("B", function () { return "$$$"; })', '"A$$$C"');
shouldBe('"ABC".replace("B", function () { return "$$$$"; })', '"A$$$$C"');
shouldBe('"ABC".replace("B", function () { return "$1"; })', '"A$1C"');
shouldBe('"ABC".replace("B", function () { return "$2"; })', '"A$2C"');

shouldBe('"John Doe".replace(/(\\w+)\\s(\\w+)/, "$2 $1")', '"Doe John"');
shouldBe('"John Doe".replace(/(\\w+)\\s(\\w+)/, function () { return "$2 $1"; })', '"$2 $1"');