
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentnormalizedocument08";
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
Add two CDATASections containing "]]>" perform normalization with split-cdata-sections=true.
Should result in two warnings and at least 4 nodes.

* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-normalizeDocument
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-split-cdata-sections
*/
function documentnormalizedocument08() {
   var success;
    if(checkInitialization(builder, "documentnormalizedocument08") != null) return;
    var doc;
      var elem;
      var domConfig;
      var elemList;
      var newChild;
      var oldChild;
      var retval;
      errorMonitor = new DOMErrorMonitor();
      
      var errors = new Array();

      var error;
      var length;
      var childNodes;
      var type;
      var splittedCount = 0;
      var severity;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      elemList = doc.getElementsByTagName("p");
      elem = elemList.item(0);
      newChild = doc.createCDATASection("this is not ]]> good");
      oldChild = elem.firstChild;

      retval = elem.replaceChild(newChild,oldChild);
      newChild = doc.createCDATASection("this is not ]]> good");
      retval = elem.appendChild(newChild);
      domConfig = doc.domConfig;

      domConfig.setParameter("split-cdata-sections", true);
	 domConfig.setParameter("error-handler", errorMonitor.handleError);
	 doc.normalizeDocument();
      errors = errorMonitor.allErrors;
for(var indexN100A3 = 0;indexN100A3 < errors.length; indexN100A3++) {
      error = errors[indexN100A3];
      type = error.type;

      severity = error.severity;

      
	if(
	("cdata-sections-splitted" == type)
	) {
	splittedCount += 1;

	}
	
		else {
			assertEquals("anyOthersShouldBeWarnings",1,severity);
       
		}
	
	}
   assertEquals("twoSplittedWarning",2,splittedCount);
       elemList = doc.getElementsByTagName("p");
      elem = elemList.item(0);
      childNodes = elem.childNodes;

      length = childNodes.length;

      	assertTrue("atLeast4ChildNodes",
      
	(length > 3)
);

}




function runTest() {
   documentnormalizedocument08();
}
