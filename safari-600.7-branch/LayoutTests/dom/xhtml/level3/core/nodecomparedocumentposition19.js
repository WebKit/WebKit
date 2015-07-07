
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodecomparedocumentposition19";
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
       setImplementationAttribute("coalescing", false);
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
	The method compareDocumentPosition compares a node with this node with regard to their position in the 
	document and according to the document order.
	
	Using compareDocumentPosition check if the document position of the first CDATASection node
	of the second element whose localName is name compared with the second CDATASection node
	is PRECEDING and is FOLLOWING vice versa.

* @author IBM
* @author Jenny Hsu
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Node3-compareDocumentPosition
*/
function nodecomparedocumentposition19() {
   var success;
    if(checkInitialization(builder, "nodecomparedocumentposition19") != null) return;
    var doc;
      var elemList;
      var elemStrong;
      var cdata1;
      var cdata2;
      var aNode;
      var cdata1Position;
      var cdata2Position;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      elemList = doc.getElementsByTagNameNS("*","strong");
      elemStrong = elemList.item(1);
      cdata2 = elemStrong.lastChild;

      aNode = cdata2.previousSibling;

      cdata1 = aNode.previousSibling;

      cdata1Position = cdata1.compareDocumentPosition(cdata2);
      assertEquals("nodecomparedocumentposition19_cdata2Follows",4,cdata1Position);
       cdata2Position = cdata2.compareDocumentPosition(cdata1);
      assertEquals("nodecomparedocumentposition_cdata1Precedes",2,cdata2Position);
       
}




function runTest() {
   nodecomparedocumentposition19();
}
