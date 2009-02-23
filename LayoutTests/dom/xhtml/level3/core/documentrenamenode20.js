
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentrenamenode20";
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

      docsLoaded = 0;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      docsLoaded += preload(docRef, "doc", "barfoo");
        
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
	Invoke the renameNode method on this document node to rename a node such that its
	qualifiedName has a prefix that is "xml:html" and namespaceURI is 
	"http://www.example.com/namespace".
	Check if a NAMESPACE_ERR gets thrown.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-renameNode
*/
function documentrenamenode20() {
   var success;
    if(checkInitialization(builder, "documentrenamenode20") != null) return;
    var doc;
      var element;
      var renamedNode;
      var docElem;
      var rootNS;
      var rootTagname;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      docElem = doc.documentElement;

      rootNS = docElem.namespaceURI;

      rootTagname = docElem.tagName;

      docElem = doc.documentElement;

      rootNS = docElem.namespaceURI;

      rootTagname = docElem.tagName;

      element = doc.createElementNS(rootNS,rootTagname);
      
	{
		success = false;
		try {
            renamedNode = doc.renameNode(element,"http://www.example.com/xml","xml:html");
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 14);
		}
		assertTrue("throw_NAMESPACE_ERR",success);
	}

}




function runTest() {
   documentrenamenode20();
}
