description("Test to make sure EntityReference nodes are always treated readonly")

var xmlDoc = document.implementation.createDocument("http://www.w3.org/1999/xhtml", "html", null);
var xmlDoc2 = document.implementation.createDocument("http://www.w3.org/1999/xhtml", "html", null);
var entityReference = xmlDoc.createEntityReference("gt");

shouldThrow("xmlDoc2.adoptNode(entityReference)");
shouldBe("entityReference.ownerDocument", "xmlDoc")

// nodeValue is defined to be null for Entity Reference nodes, and thus should silently fail to modify
// Spec is ambigious as to if we should throw here or not.  I've requested clarification:
// http://lists.w3.org/Archives/Public/www-dom/2008JanMar/0009.html
shouldThrow("entityReference.nodeValue = 'foo'");
shouldBe("entityReference.nodeValue", "null");

shouldThrow("entityReference.prefix = 'foo'");
shouldBe("entityReference.prefix", "null");

shouldThrow("entityReference.textContent = 'foo'");
shouldBe("entityReference.textContent", "'>'");

var childrenBeforeFailedAppend = entityReference.childNodes.length;
shouldBe("childrenBeforeFailedAppend", "1");
var text = document.createTextNode("FAIL");
shouldThrow("entityReference.appendChild(text)");
shouldBe("entityReference.childNodes.length", "childrenBeforeFailedAppend");

childrenBeforeFailedAppend = entityReference.childNodes.length;
shouldBe("childrenBeforeFailedAppend", "1");
shouldThrow("entityReference.insertBefore(text, entityReference.firstChild)");
shouldBe("entityReference.childNodes.length", "childrenBeforeFailedAppend");

var successfullyParsed = true;
