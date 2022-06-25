
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/noderemovechild05";
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


/**
* 
	Using removeChild on this Document node attempt to remove a new DocumentType node and
	verify if the DocumentType node is null.  Attempting to remove the DocumentType
	a second type should result in a NOT_FOUND_ERR.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-1734834066
* @see http://www.w3.org/Bugs/Public/show_bug.cgi?id=417
*/
function noderemovechild05() {
   var success;
    if(checkInitialization(builder, "noderemovechild05") != null) return;
    var doc;
      var domImpl;
      var docType;
      var removedDocType;
      var nullPubId = null;

      var nullSysId = null;

      var appendedChild;
      var removedChild;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      docType = doc.doctype;

      
      try {
      removedChild = doc.removeChild(docType);
      
      } catch (ex) {
		  if (typeof(ex.code) != 'undefined') {      
       switch(ex.code) {
       case /* NOT_SUPPORTED_ERR */ 9 :
               return ;
    default:
          throw ex;
          }
       } else { 
       throw ex;
        }
         }
        assertNotNull("removedChildNotNull",removedChild);
removedDocType = doc.doctype;

      assertNull("noderemovechild05",removedDocType);
    
	{
		success = false;
		try {
            removedChild = docType.removeChild(doc);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 8);
		}
		assertTrue("NOT_FOUND_ERR_noderemovechild05",success);
	}

}




function runTest() {
   noderemovechild05();
}
