
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodecomparedocumentposition03";
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
      docsLoaded += preload(docRef, "doc", "hc_staff");
        
      var docCompRef = null;
      if (typeof(this.docComp) != 'undefined') {
        docCompRef = this.docComp;
      }
      docsLoaded += preload(docCompRef, "docComp", "hc_staff");
        
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
	Using compareDocumentPosition check if the document position of two Document nodes obtained from the 
	same xml document is disconnected, implementation specific, and that the order of these two documents
	is reserved.

* @author IBM
* @author Jenny Hsu
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Node3-compareDocumentPosition
*/
function nodecomparedocumentposition03() {
   var success;
    if(checkInitialization(builder, "nodecomparedocumentposition03") != null) return;
    var doc;
      var docComp;
      var documentPosition1;
      var documentPosition2;
      var documentPosition3;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      
      var docCompRef = null;
      if (typeof(this.docComp) != 'undefined') {
        docCompRef = this.docComp;
      }
      docComp = load(docCompRef, "docComp", "hc_staff");
      documentPosition1 = doc.compareDocumentPosition(docComp);
      assertEquals("isImplSpecificDisconnected1",33,documentPosition1);
       documentPosition2 = docComp.compareDocumentPosition(doc);
      assert("notBothPreceding",documentPosition1 != documentPosition2);
      assert("notBothFollowing",documentPosition1 != documentPosition2);
      assertEquals("isImplSpecificDisconnected2",33,documentPosition2);
       documentPosition3 = doc.compareDocumentPosition(docComp);
      assertEquals("isConsistent",documentPosition1,documentPosition3);
       
}




function runTest() {
   nodecomparedocumentposition03();
}
