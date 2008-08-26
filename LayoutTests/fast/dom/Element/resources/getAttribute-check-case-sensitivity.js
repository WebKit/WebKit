description(
"<p>This file test the behaviour of getAttribute with regard to case.</p><p>See Bug 20247: setAttributeNode() does not work when attribute name has a capital letter in it</p>"
);

function testGetAttributeCaseInsensitive()
{
    var div = document.createElement('div');
    div.setAttribute("mixedCaseAttrib", "x");

    // Do original case lookup, and lowercase lookup.
    return div.getAttribute("mixedcaseattrib");
}

shouldBe("testGetAttributeCaseInsensitive()", '"x"');

function testGetAttributeNodeMixedCase()
{
    var div = document.createElement('div');
    var a = div.ownerDocument.createAttribute("mixedCaseAttrib");
    a.nodeValue = "x";
    div.setAttributeNode(a);
    return div.getAttribute("mixedCaseAttrib");
}

shouldBe("testGetAttributeNodeMixedCase()", '"x"');
    
function testGetAttributeNodeLowerCase(div)
{
    var div = document.createElement('div');
    var a = div.ownerDocument.createAttribute("lowercaseattrib");
    a.nodeValue = "x";
    div.setAttributeNode(a);
    return div.getAttribute("lowerCaseAttrib");
}

shouldBe("testGetAttributeNodeLowerCase()", '"x"');

function testSetAttributeNodeKeepsRef(div)
{
    var div = document.createElement('div');
    var a = div.ownerDocument.createAttribute("attrib_name");
    a.nodeValue = "0";
    div.setAttributeNode(a);

    // Mutate the attribute node.
    a.nodeValue = "1";
    
    return div.getAttribute("attrib_name");
}

shouldBe("testSetAttributeNodeKeepsRef()", '"1"');

function testAttribNodeNamePreservesCase()
{
    var div = document.createElement('div');
    var a = div.ownerDocument.createAttribute("A");
    a.nodeValue = "x";
    div.setAttributeNode(a);
      
    var result = [ a.name, a.nodeName ];
    return result.join(",");
}

shouldBe("testAttribNodeNamePreservesCase()", '"A,A"');

    
function testAttribNodeNamePreservesCaseGetNode()
{
    // getAttributeNode doesnt work on DIVs, use body element
    var body = document.body;

    var a = body.ownerDocument.createAttribute("A");
    a.nodeValue = "x";

    body.setAttributeNode(a);

    a = document.body.getAttributeNode("A");
    if (!a)
        return "FAIL";

    var result = [ a.name, a.nodeName ];
    return result.join(",");
}

shouldBe("testAttribNodeNamePreservesCaseGetNode()", '"A,A"');

function testAttribNodeNamePreservesCaseGetNode2()
{
    // getAttributeNode doesnt work on DIVs, use body element
    var body = document.body;

    var a = body.ownerDocument.createAttribute("B");
    a.nodeValue = "x";

    body.setAttributeNode(a);

    a = document.body.getAttributeNode("B");
    if (!a)
        return "FAIL";

    // Now create node second time -- this time case is preserved in FF!
    a = body.ownerDocument.createAttribute("B");
    a.nodeValue = "x";
    body.setAttributeNode(a);

    a = document.body.getAttributeNode("B");
      
    var result = [ a.name, a.nodeName ];
    return result.join(",");
}

shouldBe("testAttribNodeNamePreservesCaseGetNode2()", '"B,B"');

function testAttribNodeNameGetMutate()
{
    // getAttributeNode doesnt work on DIVs, use body element.
    var body = document.body;

    var a = body.ownerDocument.createAttribute("c");
    a.nodeValue = "0";
    body.setAttributeNode(a);

    a = document.body.getAttributeNode("c");
    a.value = "1";

    a = document.body.getAttributeNode("c");

    return a.nodeValue;
}

shouldBe("testAttribNodeNameGetMutate()", '"1"');

var node = document.createElement("div");
var attrib = document.createAttribute("myAttrib");
attrib.nodeValue = "XXX";
node.setAttributeNode(attrib);

shouldBe("(new XMLSerializer).serializeToString(node)", '"<div myAttrib=\\"XXX\\"></div>"');
shouldBe("node.getAttributeNode('myAttrib').name", '"myAttrib"');
shouldBe("node.getAttributeNode('myattrib').name", '"myAttrib"');
shouldBe("attrib.name", '"myAttrib"');

successfullyParsed = true;
