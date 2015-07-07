
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodereplacechild10";
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
        
      var doc1Ref = null;
      if (typeof(this.doc1) != 'undefined') {
        doc1Ref = this.doc1;
      }
      docsLoaded += preload(doc1Ref, "doc1", "hc_staff");
        
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
	The method replaceChild replaces the child node oldChild with newChild in the list of 
	children, and returns the oldChild node.

	Using replaceChild on this Document node attempt to replace an Entity node with
	a notation node of retieved from the DTD of another document and verify if a
	NOT_FOUND_ERR or WRONG_DOCUMENT_ERR or HIERARCHY_REQUEST err is thrown.  

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-785887307
*/
function nodereplacechild10() {
   var success;
    if(checkInitialization(builder, "nodereplacechild10") != null) return;
    var doc;
      var docType;
      var entitiesMap;
      var ent;
      var doc1;
      var docType1;
      var notationsMap;
      var notation;
      var replaced;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docType = doc.doctype;

      entitiesMap = docType.entities;

      ent = entitiesMap.getNamedItem("alpha");
      
      var doc1Ref = null;
      if (typeof(this.doc1) != 'undefined') {
        doc1Ref = this.doc1;
      }
      doc1 = load(doc1Ref, "doc1", "hc_staff");
      docType1 = doc1.doctype;

      notationsMap = docType1.notations;

      notation = notationsMap.getNamedItem("notation1");
      
      try {
      replaced = doc.replaceChild(notation,ent);
      
      } catch (ex) {
		  if (typeof(ex.code) != 'undefined') {      
       switch(ex.code) {
       case /* NOT_FOUND_ERR */ 8 :
       break;
      case /* WRONG_DOCUMENT_ERR */ 4 :
       break;
      case /* HIERARCHY_REQUEST_ERR */ 3 :
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
   nodereplacechild10();
}
