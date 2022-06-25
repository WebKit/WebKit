var src = 'http://www.dummy.com/images/123x456/image.jpg';

var matches = src.match("^(.*/)([0-9]+)(x)([0-9]+)(/.*)$");
matches[2] = 111;
matches[4] = 222;
    
shouldBe("matches", "['http://www.dummy.com/images/123x456/image.jpg','http://www.dummy.com/images/',111,'x',222,'/image.jpg']");
