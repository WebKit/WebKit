
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodeisequalnode07";
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
      
      var doc1Ref = null;
      if (typeof(this.doc1) != 'undefined') {
        doc1Ref = this.doc1;
      }
      docsLoaded += preload(doc1Ref, "doc1", "hc_staff");
        
      var doc2Ref = null;
      if (typeof(this.doc2) != 'undefined') {
        doc2Ref = this.doc2;
      }
      docsLoaded += preload(doc2Ref, "doc2", "hc_staff");
        
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
	Using isEqualNode check if 2 Element nodes having the same nodeName and namespaceURI attribute
	created by two different Document objects obtained by parsing the same xml document are equal.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Node3-isEqualNode
*/
function nodeisequalnode07() {
   var success;
    if(checkInitialization(builder, "nodeisequalnode07") != null) return;
    var doc1;
      var doc2;
      var elem1;
      var elem2;
      var isEqual;
      
      var doc1Ref = null;
      if (typeof(this.doc1) != 'undefined') {
        doc1Ref = this.doc1;
      }
      doc1 = load(doc1Ref, "doc1", "hc_staff");
      
      var doc2Ref = null;
      if (typeof(this.doc2) != 'undefined') {
        doc2Ref = this.doc2;
      }
      doc2 = load(doc2Ref, "doc2", "hc_staff");
      elem1 = doc1.createElementNS("http://www.w3.org/1999/xhtml","xhtml:html");
      elem2 = doc2.createElementNS("http://www.w3.org/1999/xhtml","xhtml:html");
      isEqual = elem1.isEqualNode(elem2);
      assertTrue("nodeisequalnode07",isEqual);

}




function runTest() {
   nodeisequalnode07();
}
