
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodecomparedocumentposition40";
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
	Using compareDocumentPosition to check if the document position of the class's attribute 
	when compared with a new attribute node is implementation_specific

* @author IBM
* @author Jenny Hsu
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Node3-compareDocumentPosition
*/
function nodecomparedocumentposition40() {
   var success;
    if(checkInitialization(builder, "nodecomparedocumentposition40") != null) return;
    var doc;
      var elemList;
      var elem;
      var attr1;
      var attr2;
      var attrPosition;
      var swappedPosition;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      elemList = doc.getElementsByTagName("acronym");
      elem = elemList.item(3);
      attr1 = elem.getAttributeNode("class");
      elem.setAttributeNS("http://www.w3.org/XML/1998/namespace","xml:lang","FR-fr");
      attr2 = elem.getAttributeNode("xml:lang");
      attrPosition = attr1.compareDocumentPosition(attr2);
      assertEquals("isImplementationSpecific",32,attrPosition);
       assertEquals("otherBitsZero",0,attrPosition);
       assert("eitherFollowingOrPreceding",0 != attrPosition);
      swappedPosition = attr2.compareDocumentPosition(attr1);
      assert("onlyOnePreceding",swappedPosition != attrPosition);
      assert("onlyOneFollowing",swappedPosition != attrPosition);
      
}




function runTest() {
   nodecomparedocumentposition40();
}
