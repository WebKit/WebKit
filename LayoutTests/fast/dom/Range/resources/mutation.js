description(
"This test checks mutation of the DOM and how it affects Ranges."
);

var a;
var b;
var c;
var d;
var e;
var paragraph;
var section;
var text;
var text1;
var text2;
var text3;

function name(node)
{
    if (node == a)
        return "a";
    if (node == b)
        return "b";
    if (node == c)
        return "c";
    if (node == d)
        return "d";
    if (node == e)
        return "e";
    if (node == paragraph)
        return "paragraph";
    if (node == section)
        return "section";
    if (node == text)
        return "text";
    if (node == text1)
        return "text1";
    if (node == text2)
        return "text2";
    if (node == text3)
        return "text3";
    return "???";
}

function description(range)
{
    return name(range.startContainer) + "," + range.startOffset + " -> " + name(range.endContainer) + "," + range.endOffset + ": " + range.toString();
}

function makeRange(sc, so, ec, eo)
{
    var newRange = document.createRange();
    newRange.setStart(sc, so);
    newRange.setEnd(ec, eo);
    return newRange;
}

function setUp1()
{
    paragraph = document.createElement("p");
    text = document.createTextNode("Abcd efgh XY blah ijkl");
    paragraph.appendChild(text);
    range = makeRange(text, 11, text, 19);
}

function createTestElement(name)
{
    var element = document.createElement("em");
    var text = document.createTextNode(name);
    element.appendChild(text);
    return element;
}

function setUp2()
{
    paragraph = document.createElement("p");
    a = createTestElement("a");
    b = createTestElement("b");
    c = createTestElement("c");
    d = createTestElement("d");
    e = createTestElement("e");

    paragraph.appendChild(a);
    paragraph.appendChild(b);
    paragraph.appendChild(c);
    paragraph.appendChild(d);
    range = makeRange(paragraph, 1, paragraph, 3);
}

function setUp3()
{
    paragraph = document.createElement("p");
    a = document.createTextNode("ax");
    b = document.createTextNode("by");

    paragraph.appendChild(a);
    paragraph.appendChild(b);

    range = makeRange(a, 1, b, 1);
}

function setUp4()
{
    paragraph = document.createElement("p");
    a = document.createTextNode("abcd");

    paragraph.appendChild(a);

    range = makeRange(a, 1, a, 3);
}

function setUp5()
{
    section = document.createElement("div");
    paragraph = document.createElement("p");
    a = document.createTextNode("abcde");

    section.appendChild(paragraph);
    paragraph.appendChild(a);

    range = makeRange(a, 2, section, 1);
}

// First, tests from the DOM Level 2 specification.

// DOM Level 2 Traversal Range, 2.12.1. Insertions, example 1.
setUp1();
text.insertData(10, "inserted text");
shouldBe('description(range)', '"text,24 -> text,32: Y blah i"');

// DOM Level 2 Traversal Range, 2.12.1. Insertions, example 2.
// Firefox does not match the DOM Level 2 specification on this one.
setUp1();
text.insertData(11, "inserted text");
shouldBe('description(range)', '"text,11 -> text,32: inserted textY blah i"');

// DOM Level 2 Traversal Range, 2.12.1. Insertions, example 3.
setUp1();
text.insertData(12, "inserted text");
shouldBe('description(range)', '"text,11 -> text,32: Yinserted text blah i"');

// DOM Level 2 Traversal Range, 2.12.1. Insertions, example 4.
setUp1();
text.insertData(17, "inserted text");
shouldBe('description(range)', '"text,11 -> text,32: Y blahinserted text i"');

// Similar test at the range end.
setUp1();
text.insertData(18, "inserted text");
shouldBe('description(range)', '"text,11 -> text,32: Y blah inserted texti"');

// Similar test at the range end.
setUp1();
text.insertData(19, "inserted text");
shouldBe('description(range)', '"text,11 -> text,19: Y blah i"');

// DOM Level 2 Traversal Range, 2.12.2. Deletions, example 1.
paragraph = document.createElement("p");
text = document.createTextNode("Abcd efgh The Range ijkl");
paragraph.appendChild(text);
range = makeRange(text, 11, text, 21);
text.deleteData(5, 8);
shouldBe('description(range)', '"text,5 -> text,13:  Range i"');

// DOM Level 2 Traversal Range, 2.12.2. Deletions, example 2.
paragraph = document.createElement("p");
text = document.createTextNode("Abcd efgh The Range ijkl");
paragraph.appendChild(text);
range = makeRange(text, 11, text, 21);
text.deleteData(5, 17);
shouldBe('description(range)', '"text,5 -> text,5: "');

// DOM Level 2 Traversal Range, 2.12.2. Deletions, example 3.
// Firefox does not match the DOM Level 2 specification on this one.
// Or maybe I wrote the test wrong?
paragraph = document.createElement("p");
text1 = document.createTextNode("ABCD efgh The ");
paragraph.appendChild(text1);
em = document.createElement("em");
paragraph.appendChild(em);
text2 = document.createTextNode("Range");
em.appendChild(text2);
text3 = document.createTextNode(" ijkl");
paragraph.appendChild(text3);
range = makeRange(text1, 11, text2, 5);
makeRange(text1, 5, text2, 1).deleteContents();
shouldBe('description(range)', '"text1,5 -> text2,4: ange"');

// DOM Level 2 Traversal Range, 2.12.2. Deletions, example 4.
paragraph = document.createElement("p");
text = document.createTextNode("Abcd efgh The Range ijkl");
paragraph.appendChild(text);
range = makeRange(text, 11, text, 21);
text.deleteData(5, 6);
shouldBe('description(range)', '"text,5 -> text,15: he Range i"');

// DOM Level 2 Traversal Range, 2.12.2. Deletions, example 5.
paragraph = document.createElement("p");
text1 = document.createTextNode("Abcd ");
paragraph.appendChild(text1);
em = document.createElement("em");
paragraph.appendChild(em);
text2 = document.createTextNode("efgh The Range ij");
em.appendChild(text2);
text3 = document.createTextNode("kl");
paragraph.appendChild(text3);
range = makeRange(text2, 6, text2, 16);
makeRange(paragraph, 1, paragraph, 2).deleteContents();
shouldBe('paragraph.childNodes.length', '2');
shouldBe('text1.length', '5');
shouldBe('description(range)', '"paragraph,1 -> paragraph,1: "');
paragraph.normalize();
shouldBe('paragraph.childNodes.length', '1');
shouldBe('text1.length', '7');
shouldBe('description(range)', '"text1,5 -> text1,5: "');

// Inserting a node in the start container, before the range start offset.
setUp2();
paragraph.insertBefore(e, a);
shouldBe('description(range)', '"paragraph,2 -> paragraph,4: bc"');

// Inserting a node in the start container, at the range start offset.
setUp2();
paragraph.insertBefore(e, b);
shouldBe('description(range)', '"paragraph,1 -> paragraph,4: ebc"');

// Inserting a node in the start container, between start and end.
setUp2();
paragraph.insertBefore(e, c);
shouldBe('description(range)', '"paragraph,1 -> paragraph,4: bec"');

// Inserting a node in the end container, at the range end offset.
setUp2();
paragraph.insertBefore(e, d);
shouldBe('description(range)', '"paragraph,1 -> paragraph,3: bc"');

// Inserting a node in the end container, after the range end offset.
setUp2();
paragraph.appendChild(e);
shouldBe('description(range)', '"paragraph,1 -> paragraph,3: bc"');

// Removing the start container of a range.
setUp2();
range = makeRange(b, 0, d, 1);
paragraph.removeChild(b);
shouldBe('description(range)', '"paragraph,1 -> d,1: cd"');
setUp2();
range = makeRange(b, 1, d, 0);
paragraph.removeChild(b);
shouldBe('description(range)', '"paragraph,1 -> d,0: c"');

// Removing the end container of a range.
setUp2();
range = makeRange(b, 0, d, 1);
paragraph.removeChild(d);
shouldBe('description(range)', '"b,0 -> paragraph,3: bc"');
setUp2();
range = makeRange(b, 1, d, 0);
paragraph.removeChild(d);
shouldBe('description(range)', '"b,1 -> paragraph,3: c"');

// Calling normalize with a range that has an endpoint in a text node that gets merged into another.
// Firefox does not do what the DOM Level 2 specification seems to call for on this one.
setUp3();
paragraph.normalize();
shouldBe('description(range)', '"a,1 -> a,3: xb"');

// Calling splitText when a range has an endpoint on in the piece that gets made into a new text node.
// Firefox does not do what the DOM Level 2 specification seems to call for on this one.
setUp4();
b = a.splitText(1);
shouldBe('description(range)', '"a,1 -> b,2: bc"');
setUp4();
b = a.splitText(2);
shouldBe('description(range)', '"a,1 -> b,1: bc"');
setUp4();
b = a.splitText(3);
shouldBe('description(range)', '"a,1 -> a,3: bc"');

// Range test in Acid3.
setUp5();
shouldBe('description(range)', '"a,2 -> section,1: cde"');
section.removeChild(paragraph);
shouldBe('description(range)', '"section,0 -> section,0: "');

// Children in a range being removed, but by DOM mutation (not CharacterData mutation).
// Test CharacterData.replaceData cases?
// Test Element.innerHTML cases (setting it)?
// Test Element.innerText cases (setting it)?
// Test Element.outerHTML cases (setting it)?
// Test Element.outerText cases (setting it)?
// Test Node.replaceChild cases?
// Test cases where Node.insertBefore/appendChild has a side effect of deleting the node from a range.
// Test Range.deleteContents cases?
// Test Range.extractContents cases?
// Test Range.surroundContents cases?
// Test Text.replaceWholeText cases?

var successfullyParsed = true;
