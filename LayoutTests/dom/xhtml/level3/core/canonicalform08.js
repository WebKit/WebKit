
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/canonicalform08";
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
       setImplementationAttribute("validating", false);

      docsLoaded = 0;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      docsLoaded += preload(docRef, "doc", "canonicalform01");
        
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
Normalize document based on section 3.1 with canonical-form set to true and check normalized document.

* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-normalizeDocument
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-canonical-form
*/
function canonicalform08() {
   var success;
    if(checkInitialization(builder, "canonicalform08") != null) return;
    var doc;
      var bodyList;
      var body;
      var domConfig;
      var canSet;
      var canSetValidate;
      errorMonitor = new DOMErrorMonitor();
      
      var node;
      var nodeName;
      var nodeValue;
      var nodeType;
      var length;
      var text;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "canonicalform01");
      domConfig = doc.domConfig;

      canSet = domConfig.canSetParameter("canonical-form",true);
      
	if(
	canSet
	) {
	domConfig.setParameter("canonical-form", true);
	 domConfig.setParameter("error-handler", errorMonitor.handleError);
	 doc.normalizeDocument();
      errorMonitor.assertLowerSeverity("normalizeError", 2);
     node = doc.firstChild;

      nodeType = node.nodeType;

      assertEquals("PIisFirstChild",7,nodeType);
       nodeValue = node.data;

      length = nodeValue.length;
      assertEquals("piDataLength",36,length);
       node = node.nextSibling;

      nodeType = node.nodeType;

      assertEquals("TextisSecondChild",3,nodeType);
       nodeValue = node.nodeValue;

      length = nodeValue.length;
      assertEquals("secondChildLength",1,length);
       node = node.nextSibling;

      nodeType = node.nodeType;

      assertEquals("ElementisThirdChild",1,nodeType);
       node = node.nextSibling;

      nodeType = node.nodeType;

      assertEquals("TextisFourthChild",3,nodeType);
       nodeValue = node.nodeValue;

      length = nodeValue.length;
      assertEquals("fourthChildLength",1,length);
       node = node.nextSibling;

      nodeType = node.nodeType;

      assertEquals("PIisFifthChild",7,nodeType);
       nodeValue = node.data;

      assertEquals("trailingPIData","",nodeValue);
       node = node.nextSibling;

      nodeType = node.nodeType;

      assertEquals("TextisSixthChild",3,nodeType);
       nodeValue = node.nodeValue;

      length = nodeValue.length;
      assertEquals("sixthChildLength",1,length);
       node = node.nextSibling;

      nodeType = node.nodeType;

      assertEquals("CommentisSeventhChild",8,nodeType);
       node = node.nextSibling;

      nodeType = node.nodeType;

      assertEquals("TextisEighthChild",3,nodeType);
       nodeValue = node.nodeValue;

      length = nodeValue.length;
      assertEquals("eighthChildLength",1,length);
       node = node.nextSibling;

      nodeType = node.nodeType;

      assertEquals("CommentisNinthChild",8,nodeType);
       node = node.nextSibling;

      assertNull("TenthIsNull",node);
    
	}
	
}




function runTest() {
   canonicalform08();
}
