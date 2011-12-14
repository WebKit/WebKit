
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodecomparedocumentposition26";
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
	Using compareDocumentPosition check if the EntityReference node contains and precedes it's first
	childElement, and that the childElement is contained and follows the EntityReference node.

* @author IBM
* @author Jenny Hsu
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Node3-compareDocumentPosition
*/
function nodecomparedocumentposition26() {
   var success;
    if(checkInitialization(builder, "nodecomparedocumentposition26") != null) return;
    var doc;
      var varList;
      var varElem;
      var entRef;
      var entRefChild1;
      var entRefPosition;
      var entRefChild1Position;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      
	if(
	(getImplementationAttribute("expandEntityReferences") == false)
	) {
	varList = doc.getElementsByTagName("var");
      varElem = varList.item(2);
      assertNotNull("varElemNotNull",varElem);
entRef = varElem.firstChild;

      assertNotNull("entRefNotNull",entRef);

	}
	
		else {
			entRef = doc.createEntityReference("ent4");
      
		}
	entRefChild1 = entRef.firstChild;

      assertNotNull("entRefChild1NotNull",entRefChild1);
entRefPosition = entRef.compareDocumentPosition(entRefChild1);
      assertEquals("nodecomparedocumentpositionIsContainedFollowing26",20,entRefPosition);
       entRefChild1Position = entRefChild1.compareDocumentPosition(entRef);
      assertEquals("nodecomparedocumentpositionContainsPRECEDING26",10,entRefChild1Position);
       
}




function runTest() {
   nodecomparedocumentposition26();
}
