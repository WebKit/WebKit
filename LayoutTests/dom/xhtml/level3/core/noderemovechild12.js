
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/noderemovechild12";
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
	The method removeChild removes the child node indicated by oldChild from the list 
	of children, and returns it. 

	Using removeChild on a new DocumentFragment node attempt to remove a new EntityReference node.
        Also attempt to remove the document fragment node from the EntityReference.  Verify that a 
        NO_MODIFICATION_ALLOWED_ERR (EntityReference node is read-only) or a NOT_FOUND_ERR is thrown.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-1734834066
*/
function noderemovechild12() {
   var success;
    if(checkInitialization(builder, "noderemovechild12") != null) return;
    var doc;
      var docFrag;
      var eRef;
      var removedERef;
      var appendedChild;
      var removedChild;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docFrag = doc.createDocumentFragment();
      eRef = doc.createEntityReference("ent1");
      appendedChild = docFrag.appendChild(eRef);
      removedChild = docFrag.removeChild(eRef);
      removedERef = docFrag.firstChild;

      assertNull("noderemovechild12",removedERef);
    
      try {
      removedChild = eRef.removeChild(docFrag);
      
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
   noderemovechild12();
}
