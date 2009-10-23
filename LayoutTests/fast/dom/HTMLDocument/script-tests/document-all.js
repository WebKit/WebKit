var htmlallcollection = document.all;
shouldBe('htmlallcollection.toString()', "'[object HTMLAllCollection]'");
shouldBeTrue('typeof htmlallcollection.tags == "function"');
shouldBe('htmlallcollection.tags("body").length', "1");

successfullyParsed = true;
