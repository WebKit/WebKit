// this function creates element with specified property and contents.
function createElement(type, props, contents) {
    if (window.testRunner)
      testRunner.dumpAsText();

    var element = document.createElement(type);

    debug('Created element of type: ' + type);
    for (var i in props) {
      if (props.hasOwnProperty(i)) {
        debug('Set attribute: ' + i + ', value: ' + props[i]);
        element.setAttribute(i,props[i]);
      }
    }
    if (contents) {
      element.innerHTML = contents;
    }
    return element;
}


// runs a test and writes a log
function runTest(collection, elements, title) {
  if (window.testRunner)
    testRunner.dumpAsText();

  pass = true;

  if (collection.length != elements.length) {
    pass = false
    debug('FAIL - expected ' + elements.length + ' elements but got ' + collection.length + " elements. ");
  }
  for(var i = 0, max = collection.length > elements.length ? elements.length : collection.length; i < max; i++) {
    if(collection[i] != elements[i]) {
      debug(title);
      pass = false
      debug('FAIL - expected element : ' + elements[i].tagName + " but got element :" + collection[i].tagName);
      debug('FAIL - expected element with id: ' + elements[i].id + " but got element with id:" + collection[i].id);
    }
  }
  if (pass)
    debug(title + ': PASS');
}
