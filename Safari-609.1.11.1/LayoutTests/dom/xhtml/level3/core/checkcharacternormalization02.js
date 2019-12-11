
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/checkcharacternormalization02";
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
Normalize document with check-character-normalization set to true, check that
non-normalized characters are signaled.

* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-normalizeDocument
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-check-character-normalization
* @see http://www.w3.org/TR/2003/WD-charmod-20030822/
*/
function checkcharacternormalization02() {
   var success;
    if(checkInitialization(builder, "checkcharacternormalization02") != null) return;
    var doc;
      var docElem;
      var domConfig;
      errorMonitor = new DOMErrorMonitor();
      
      var pList;
      var pElem;
      var text;
      var textValue;
      var retval;
      var canSet;
      var errors = new Array();

      var error;
      var severity;
      var locator;
      var relatedNode;
      var errorCount = 0;
      var errorType;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      domConfig = doc.domConfig;

      canSet = domConfig.canSetParameter("check-character-normalization",true);
      
	if(
	canSet
	) {
	domConfig.setParameter("check-character-normalization", true);
	 domConfig.setParameter("error-handler", errorMonitor.handleError);
	 pList = doc.getElementsByTagName("p");
      pElem = pList.item(0);
      text = doc.createTextNode("suçon");
      retval = pElem.appendChild(text);
      doc.normalizeDocument();
      errors = errorMonitor.allErrors;
for(var indexN100AA = 0;indexN100AA < errors.length; indexN100AA++) {
      error = errors[indexN100AA];
      severity = error.severity;

      
	if(
	(2 == severity)
	) {
	errorCount += 1;
errorType = error.type;

      assertEquals("errorType","check-character-normalization-failure",errorType);
       locator = error.location;

      relatedNode = locator.relatedNode;

      assertSame("relatedNodeSame",text,relatedNode);

	}
	
	}
   assertEquals("oneError",1,errorCount);
       
	}
	
}




function runTest() {
   checkcharacternormalization02();
}
