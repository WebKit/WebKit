
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodeinsertbefore25";
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
       setImplementationAttribute("expandEntityReferences", false);

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
	Using insertBefore on a child Element of an EntityReference node attempt to insert 
	a new Element node, before a Text node child of an Entity Node's replacement 
	text and verify if a NO_MODIFICATION_ALLOWED_ERR is raised.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-952280727
*/
function nodeinsertbefore25() {
   var success;
    if(checkInitialization(builder, "nodeinsertbefore25") != null) return;
    var doc;
      var element;
      var eRef;
      var span;
      var spanText;
      var newNode;
      var childList;
      var inserted;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      childList = doc.getElementsByTagName("var");
      element = childList.item(2);
      eRef = element.firstChild;

      span = eRef.firstChild;

      assertNotNull("spanNotNull",span);
spanText = span.firstChild;

      assertNotNull("spanTextNotNull",spanText);
newNode = doc.createElementNS("http://www.w3.org/1999/xhtml","span");
      
	{
		success = false;
		try {
            inserted = span.insertBefore(newNode,spanText);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 7);
		}
		assertTrue("throw_NO_MODIFICATION_ALLOWED_ERR",success);
	}

}




function runTest() {
   nodeinsertbefore25();
}
