
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/noderemovechild07";
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
Attempts to remove a notation from a Document node.  Since notations are children of 
DocumentType, not Document the operation should fail with a NOT_FOUND_ERR.  Attempting
to remove Document from a Notation should also fail either with a NOT_FOUND_ERR
or a NO_MODIFICATION_ALLOWED_ERR.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-1734834066
* @see http://www.w3.org/Bugs/Public/show_bug.cgi?id=418
*/
function noderemovechild07() {
   var success;
    if(checkInitialization(builder, "noderemovechild07") != null) return;
    var doc;
      var docType;
      var notations;
      var notation;
      var removedChild;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docType = doc.doctype;

      notations = docType.notations;

      notation = notations.getNamedItem("notation1");
      
	{
		success = false;
		try {
            removedChild = doc.removeChild(notation);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 8);
		}
		assertTrue("NOT_FOUND_ERR_noderemovechild07_1",success);
	}

      try {
      removedChild = notation.removeChild(doc);
      
      } catch (ex) {
		  if (typeof(ex.code) != 'undefined') {      
       switch(ex.code) {
       case /* NOT_FOUND_ERR */ 8 :
       break;
      case /* NO_MODIFICATION_ALLOWED_ERR */ 7 :
       break;
          default:
          throw ex;
          }
       } else { 
       throw ex;
        }
         }
        
}




function runTest() {
   noderemovechild07();
}
