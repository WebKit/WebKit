// this function creates element with specified property and contents.
function createElem(type,props,contents) {
    if (window.layoutTestController)
      layoutTestController.dumpAsText();

    var elem = document.createElement(type);

    for( var i in props ) {
      if( props.hasOwnProperty(i) ) {
        elem.setAttribute(i,props[i]);
      }
    }
    if( contents ) {
      elem.innerHTML = contents;
    }
    return elem;
}


// runs a test and writes a log
function runTest(collection, elements, title) {
  if (window.layoutTestController)
    layoutTestController.dumpAsText();

  document.write(title + ': ');
  pass = true;

  if (collection.length != elements.length) {
    pass = false
    document.write('FAIL - expected ' + elements.length + ' elements but got ' + collection.length + " elements. ");
  }
  for(var i = 0, max = collection.length > elements.length ? elements.length : collection.length; i < max; i++) {
    if(collection[i] != elements[i]) {
      pass = false
      document.write('FAIL - expected element : ' + elements[i].tagName + " but got element :" + collection[i].tagName);
      document.write('FAIL - expected element with id: ' + elements[i].id + " but got element with id:" + collection[i].id);
    }
  }
  if (pass)
    document.write('PASS');
  document.write('<br>');
}
