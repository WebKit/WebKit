
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentrenamenode28";
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
	Invoke the renameNode method to attempt to rename a Entity and Notation nodes of this Document.
	Check if a NOT_SUPPORTED_ERR gets thrown.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-renameNode
*/
function documentrenamenode28() {
   var success;
    if(checkInitialization(builder, "documentrenamenode28") != null) return;
    var doc;
      var docType;
      var entityNodeMap;
      var notationNodeMap;
      var entity;
      var notation;
      var renamedEntityNode;
      var renamedNotationNode;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docType = doc.doctype;

      entityNodeMap = docType.entities;

      notationNodeMap = docType.notations;

      entity = entityNodeMap.getNamedItem("alpha");
      notation = notationNodeMap.getNamedItem("notation1");
      
	{
		success = false;
		try {
            renamedEntityNode = doc.renameNode(entity,"http://www.w3.org/DOM/Test","beta");
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 9);
		}
		assertTrue("documentrenamenode28_ENTITY_NOT_SUPPORTED_ERR",success);
	}

	{
		success = false;
		try {
            renamedNotationNode = doc.renameNode(notation,"http://www.w3.org/DOM/Test","notation2");
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 9);
		}
		assertTrue("documentrenamenode28_NOTATION_NOT_SUPPORTED_ERR",success);
	}

}




function runTest() {
   documentrenamenode28();
}
