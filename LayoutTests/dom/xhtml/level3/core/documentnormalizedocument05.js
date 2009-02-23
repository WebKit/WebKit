
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentnormalizedocument05";
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
Add a L1 element to a L2 namespace aware document and perform namespace normalization.  Should result
in an error.

* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-normalizeDocument
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms#normalizeDocumentAlgo
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-namespaces
*/
function documentnormalizedocument05() {
   var success;
    if(checkInitialization(builder, "documentnormalizedocument05") != null) return;
    var doc;
      var elem;
      var domConfig;
      var pList;
      var newChild;
      var retval;
      errorMonitor = new DOMErrorMonitor();
      
      var errors = new Array();

      var error;
      var errorCount = 0;
      var severity;
      var problemNode;
      var location;
      var lineNumber;
      var columnNumber;
      var byteOffset;
      var utf16Offset;
      var uri;
      var type;
      var message;
      var relatedException;
      var relatedData;
      var length;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      pList = doc.getElementsByTagName("p");
      elem = pList.item(0);
      newChild = doc.createElement("br");
      retval = elem.appendChild(newChild);
      domConfig = doc.domConfig;

      domConfig.setParameter("namespaces", true);
	 domConfig.setParameter("error-handler", errorMonitor.handleError);
	 doc.normalizeDocument();
      errors = errorMonitor.allErrors;
for(var indexN100B6 = 0;indexN100B6 < errors.length; indexN100B6++) {
      error = errors[indexN100B6];
      severity = error.severity;

      
	if(
	(2 == severity)
	) {
	location = error.location;

      problemNode = location.relatedNode;

      assertSame("relatedNodeIsL1Node",newChild,problemNode);
lineNumber = location.lineNumber;

      assertEquals("lineNumber",-1,lineNumber);
       columnNumber = location.columnNumber;

      assertEquals("columnNumber",-1,columnNumber);
       byteOffset = location.byteOffset;

      assertEquals("byteOffset",-1,byteOffset);
       utf16Offset = location.utf16Offset;

      assertEquals("utf16Offset",-1,utf16Offset);
       uri = location.uri;

      assertNull("uri",uri);
    message = error.message;

      length = message.length;
      	assertTrue("messageNotEmpty",
      
	(length > 0)
);
type = error.type;

      relatedData = error.relatedData;

      relatedException = error.relatedException;

      errorCount += 1;

	}
	
		else {
			assertEquals("anyOthersShouldBeWarnings",1,severity);
       
		}
	
	}
   assertEquals("oneError",1,errorCount);
       
}




function runTest() {
   documentnormalizedocument05();
}
