description(
'This test checks whether passing wrong types to setAttributeNode causes a crash.'
);

var element = document.createElement("input");

shouldThrow('element.setAttributeNode("style");');
shouldThrow('element.setAttributeNode(null);');
shouldThrow('element.setAttributeNode(undefined);');
shouldThrow('element.setAttributeNode(new Object);');
shouldThrow('element.removeAttributeNode("style");');
shouldThrow('element.removeAttributeNode(null);');
shouldThrow('element.removeAttributeNode(undefined);');
shouldThrow('element.removeAttributeNode(new Object);');
shouldThrow('element.setAttributeNodeNS("style");');
shouldThrow('element.setAttributeNodeNS(null);');
shouldThrow('element.setAttributeNodeNS(undefined);');
shouldThrow('element.setAttributeNodeNS(new Object);');

var successfullyParsed = true;
