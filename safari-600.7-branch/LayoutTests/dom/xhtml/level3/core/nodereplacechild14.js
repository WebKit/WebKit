
/*
Copyright Â© 2001-2004 World Wide Web Consortium, 
(Massachusetts Institute of Technology, European Research Consortium 
for Informatics and Mathematics, Keio University). All 
Rights Reserved. This work is distributed under the W3CÂ® Software License [1] in the 
hope that it will be useful, but WITHOUT ANY WARRANTY; without even 
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 

[1] http://www.w3.org/Consortium/Legal/2002/copyright-software-20021231
*/



   /**
    *  Gets URI that identifies the test.
    *  @return uri identifier of test
    */
function getTargetURI() {
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodereplacechild14";
   }

var docsLoaded = -1000000;
var builder = null;

//
//   This function is called by the testing framework before
//      running the test suite.
//
//   If there are no configuration exceptions, asynchronous
//        document loading is started.  Otherwise, the status
//        is set to complete and the exception is immediately
//        raised when entering the body of the test.
//
function setUpPage() {
   setUpPageStatus = 'running';
   try {
     //
     //   creates test document builder, may throw exception
     //
     builder = createConfiguredBuilder();
       setImplementationAttribute("namespaceAware", true);

      docsLoaded = 0;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      docsLoaded += preload(docRef, "doc", "hc_staff");
        
       if (docsLoaded == 1) {
          setUpPageStatus = 'complete';
       }
    } catch(ex) {
    	catchInitializationError(builder, ex);
        setUpPageStatus = 'complete';
    }
}



//
//   This method is called on the completion of 
//      each asychronous load started in setUpTests.
//
//   When every synchronous loaded document has completed,
//      the page status is changed which allows the
//      body of the test to be executed.
function loadComplete() {
    if (++docsLoaded == 1) {
        setUpPageStatus = 'complete';
    }
}


/**
* 
	The method replaceChild replaces the child node oldChild with newChild in the list of 
	children, and returns the oldChild node.

	Using replaceChild on the documentElement of a newly created Document node, attempt to replace an
        element child of this documentElement node with a child that was imported from another document.  
        Verify the nodeName of the replaced element node. 

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-785887307
*/
function nodereplacechild14() {
   var success;
    if(checkInitialization(builder, "nodereplacechild14") != null) return;
    var doc;
      var newDoc;
      var docElem;
      var elem;
      var elem2;
      var imported;
      var replaced;
      var domImpl;
      var nodeName;
      var appendedChild;
      var nullDocType = null;

      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      elem = doc.createElementNS("http://www.w3.org/DOM/Test","dom3:doc1elem");
      domImpl = doc.implementation;
newDoc = domImpl.createDocument("http://www.w3.org/DOM/test","dom3:doc",nullDocType);
      elem2 = newDoc.createElementNS("http://www.w3.org/DOM/Test","dom3:doc2elem");
      imported = newDoc.importNode(elem,true);
      docElem = newDoc.documentElement;

      appendedChild = docElem.appendChild(imported);
      appendedChild = docElem.appendChild(elem2);
      replaced = docElem.replaceChild(imported,elem2);
      nodeName = replaced.nodeName;

      assertEquals("nodereplacechild14","dom3:doc2elem",nodeName);
       
}




function runTest() {
   nodereplacechild14();
}
