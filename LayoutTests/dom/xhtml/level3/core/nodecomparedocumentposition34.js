
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodecomparedocumentposition34";
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
	Create a new Element node, add new Text, Element and Processing Instruction nodes to it.
	Using compareDocumentPosition, compare the position of the Element with respect to the Text
	and the Text with respect to the Processing Instruction.

* @author IBM
* @author Jenny Hsu
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Node3-compareDocumentPosition
*/
function nodecomparedocumentposition34() {
   var success;
    if(checkInitialization(builder, "nodecomparedocumentposition34") != null) return;
    var doc;
      var elemMain;
      var elem;
      var txt;
      var pi;
      var elementToTxtPosition;
      var txtToPiPosition;
      var appendedChild;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      elemMain = doc.createElementNS("http://www.w3.org/1999/xhtml","p");
      elem = doc.createElementNS("http://www.w3.org/1999/xhtml","br");
      txt = doc.createTextNode("TEXT");
      pi = doc.createProcessingInstruction("PIT","PID");
      appendedChild = elemMain.appendChild(txt);
      appendedChild = elemMain.appendChild(elem);
      appendedChild = elemMain.appendChild(pi);
      elementToTxtPosition = txt.compareDocumentPosition(elem);
      assertEquals("nodecomparedocumentpositionFollowing34",4,elementToTxtPosition);
       txtToPiPosition = pi.compareDocumentPosition(txt);
      assertEquals("nodecomparedocumentpositionPRECEDING34",2,txtToPiPosition);
       
}




function runTest() {
   nodecomparedocumentposition34();
}
