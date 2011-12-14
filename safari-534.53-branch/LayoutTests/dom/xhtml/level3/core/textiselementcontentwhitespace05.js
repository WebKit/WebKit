
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/textiselementcontentwhitespace05";
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

//DOMErrorMonitor's require a document level variable named errorMonitor
var errorMonitor;
	 
/**
* 
Replace the whitespace before the "p" element in barfoo with non-whitespace and normalize with validation.
isElementContentWhitespace should be false since the node is not whitespace.  

* @author Curt Arnold
* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Text3-isElementContentWhitespace
*/
function textiselementcontentwhitespace05() {
   var success;
    if(checkInitialization(builder, "textiselementcontentwhitespace05") != null) return;
    var doc;
      var bodyList;
      var bodyElem;
      var textNode;
      var nonBlankNode;
      var returnedNode;
      var isElemContentWhitespace;
      var domConfig;
      var canSetValidation;
      var refChild;
      errorMonitor = new DOMErrorMonitor();
      
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      domConfig = doc.domConfig;

      canSetValidation = domConfig.canSetParameter("validate",true);
      
	if(
	canSetValidation
	) {
	domConfig.setParameter("validate", true);
	 domConfig.setParameter("error-handler", errorMonitor.handleError);
	 bodyList = doc.getElementsByTagName("body");
      bodyElem = bodyList.item(0);
      refChild = bodyElem.firstChild;

      nonBlankNode = doc.createTextNode("not blank");
      returnedNode = bodyElem.insertBefore(nonBlankNode,refChild);
      doc.normalizeDocument();
      errorMonitor.assertLowerSeverity("noErrors", 2);
     bodyList = doc.getElementsByTagName("body");
      bodyElem = bodyList.item(0);
      textNode = bodyElem.firstChild;

      isElemContentWhitespace = textNode.isElementContentWhitespace;

      assertFalse("notElemContent",isElemContentWhitespace);

	}
	
}




function runTest() {
   textiselementcontentwhitespace05();
}
