description('Test of normalize on an XML document with CDATA.');

var parser = new DOMParser();
var serializer = new XMLSerializer();

var xmlChunk = parser.parseFromString(
    '<foo>' +
    'This is some text before the CDATA' +
    '<![CDATA[This is some <bold>markup</bold> inside of a CDATA]]>' +
    'This is some text after the CDATA' +
    '</foo>',
    'application/xml');

debug('Before normalize');
shouldBe('serializer.serializeToString(xmlChunk)', '"<foo>This is some text before the CDATA<![CDATA[This is some <bold>markup</bold> inside of a CDATA]]>This is some text after the CDATA</foo>"');
shouldBe('xmlChunk.documentElement.childNodes.length', '3');
xmlChunk.documentElement.normalize();
debug('After normalize');
shouldBe('serializer.serializeToString(xmlChunk)', '"<foo>This is some text before the CDATA<![CDATA[This is some <bold>markup</bold> inside of a CDATA]]>This is some text after the CDATA</foo>"');
shouldBe('xmlChunk.documentElement.childNodes.length', '3');

var successfullyParsed = true;
