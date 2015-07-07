
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodereplacechild27";
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
       setImplementationAttribute("expandEntityReferences", false);

      docsLoaded = 0;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      docsLoaded += preload(docRef, "doc", "hc_staff");
        
      var doc2Ref = null;
      if (typeof(this.doc2) != 'undefined') {
        doc2Ref = this.doc2;
      }
      docsLoaded += preload(doc2Ref, "doc2", "hc_staff");
        
       if (docsLoaded == 2) {
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
    if (++docsLoaded == 2) {
        setUpPageStatus = 'complete';
    }
}


/**
* 
	The method replaceChild replaces the child node oldChild with newChild in the list of 
	children, and returns the oldChild node.

	Using replaceChild on an Element node attempt to replace an Element node with another 
	Element from another document and verify if a WRONG_DOCUMENT_ERR gets thrown.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-785887307
*/
function nodereplacechild27() {
   var success;
    if(checkInitialization(builder, "nodereplacechild27") != null) return;
    var doc;
      var doc2;
      var childList;
      var childList2;
      var elem2;
      var elem;
      var firstChild;
      var nodeName;
      var replaced;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      childList = doc.getElementsByTagNameNS("*","p");
      elem = childList.item(0);
      firstChild = elem.firstChild;

      
      var doc2Ref = null;
      if (typeof(this.doc2) != 'undefined') {
        doc2Ref = this.doc2;
      }
      doc2 = load(doc2Ref, "doc2", "hc_staff");
      childList2 = doc2.getElementsByTagNameNS("*","p");
      elem2 = childList2.item(0);
      
	{
		success = false;
		try {
            replaced = elem.replaceChild(elem2,firstChild);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 4);
		}
		assertTrue("WRONG_DOCUMENT_ERR_nodereplacechild27",success);
	}

}




function runTest() {
   nodereplacechild27();
}
