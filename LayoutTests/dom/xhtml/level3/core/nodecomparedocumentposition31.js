
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodecomparedocumentposition31";
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
	Using compareDocumentPosition to check if invoking the method on the first name node with
	a new node appended to the second position node as a parameter is FOLLOWING, and is PRECEDING vice versa

* @author IBM
* @author Jenny Hsu
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Node3-compareDocumentPosition
*/
function nodecomparedocumentposition31() {
   var success;
    if(checkInitialization(builder, "nodecomparedocumentposition31") != null) return;
    var doc;
      var nameList;
      var positionList;
      var strong;
      var code;
      var newElem;
      var namePosition;
      var elemPosition;
      var appendedChild;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      nameList = doc.getElementsByTagName("strong");
      strong = nameList.item(0);
      positionList = doc.getElementsByTagName("code");
      code = positionList.item(1);
      newElem = doc.createElementNS("http://www.w3.org/1999/xhtml","br");
      appendedChild = code.appendChild(newElem);
      namePosition = strong.compareDocumentPosition(newElem);
      assertEquals("nodecomparedocumentpositionFollowing31",4,namePosition);
       elemPosition = newElem.compareDocumentPosition(strong);
      assertEquals("nodecomparedocumentpositionPRECEDING31",2,elemPosition);
       
}




function runTest() {
   nodecomparedocumentposition31();
}
