function addCell(row, contents) {
  var cell = document.createElementNS("http://www.w3.org/1999/xhtml","td");
  row.appendChild(cell);
  cell.innerHTML = contents;
}

function reportResult(tagName, tagNamespace, id, attrName, attrNamespace, sensitive, firstValue, secondValue) {
  var newRow = document.createElementNS("http://www.w3.org/1999/xhtml", 'tr');
  document.getElementById("results").appendChild(newRow);
  addCell(newRow, sensitive);
  addCell(newRow, tagName);
  addCell(newRow, tagNamespace);
  addCell(newRow, id);
  addCell(newRow, attrName);
  addCell(newRow, attrNamespace);
  addCell(newRow, firstValue);
  addCell(newRow, secondValue);
}

function resultLog(string) {
  var resultLine = document.createElementNS("http://www.w3.org/1999/xhtml","tr");
  document.getElementById("results").appendChild(resultLine);      
  var resultText = document.createElementNS("http://www.w3.org/1999/xhtml","td");
  resultLine.appendChild(resultText);
  resultText.setAttribute("colspan", '6');
  resultText.innerHTML = string;
}

function getTestElement(number) {
  return document.getElementById("test" + number);
}

function checkAttribute(element, attrName, namespace) {
  var first;
  var second;
  
  if (typeof(namespace) == 'undefined') {
    first = element.getAttribute(attrName);
    second = element.getAttribute(attrName.toUpperCase());
  } else {
    first = element.getAttributeNS(namespace, attrName);
    second = element.getAttributeNS(namespace, attrName.toUpperCase());
  }
  
  reportResult(element.localName, element.namespaceURI, element.id, attrName, namespace, (first != second), first, second);
}

function createAttributesForCheck(element, attrName, namespace) {
  if (typeof(namespace) == 'undefined') {
    element.setAttribute(attrName, 'first');
    element.setAttribute(attrName.toUpperCase(), 'second');
  } else {
    element.setAttributeNS(namespace, attrName, 'first');
    element.setAttributeNS(namespace, attrName.toUpperCase(), 'second');
  }
}

function createElementForCheck(number, name, namespace) {
  var newElement;
  if (typeof(namespace) == 'undefined')
    newElement = document.createElement(name);
  else
    newElement = document.createElementNS(namespace, name);
  
  newElement.id = "test" + number;
  document.getElementById("javascriptTests").appendChild(newElement);
  return newElement;
}

function createAndCheckAttributes(element, attrName, namespace) {
  createAttributesForCheck(element, attrName, namespace);
  checkAttribute(element, attrName, namespace);
}
