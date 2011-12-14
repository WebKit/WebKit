
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentnormalizedocument04";
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
	Append a Comment node and normalize with "comments" set to false.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-normalizeDocument
* @see http://www.w3.org/Bugs/Public/show_bug.cgi?id=416
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-comments
*/
function documentnormalizedocument04() {
   var success;
    if(checkInitialization(builder, "documentnormalizedocument04") != null) return;
    var doc;
      var elem;
      var newComment;
      var lastChild;
      var text;
      var nodeName;
      var appendedChild;
      var domConfig;
      errorMonitor = new DOMErrorMonitor();
      
      var pList;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      pList = doc.getElementsByTagName("p");
      elem = pList.item(0);
      newComment = doc.createComment("COMMENT_NODE");
      appendedChild = elem.appendChild(newComment);
      domConfig = doc.domConfig;

      domConfig.setParameter("comments", true);
	 domConfig.setParameter("error-handler", errorMonitor.handleError);
	 doc.normalizeDocument();
      errorMonitor.assertLowerSeverity("normalizationError", 2);
     pList = doc.getElementsByTagName("p");
      elem = pList.item(0);
      lastChild = elem.lastChild;

      nodeName = lastChild.nodeName;

      assertEquals("documentnormalizedocument04_true","#comment",nodeName);
       domConfig.setParameter("comments", false);
	 doc.normalizeDocument();
      errorMonitor.assertLowerSeverity("normalization2Error", 2);
     pList = doc.getElementsByTagName("p");
      elem = pList.item(0);
      lastChild = elem.lastChild;

      nodeName = lastChild.nodeName;

      assertEquals("hasChildText","#text",nodeName);
       
}




function runTest() {
   documentnormalizedocument04();
}
