
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodeappendchild01";
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
An attempt to add a second doctype node should result in a HIERARCHY_REQUEST_ERR
or a NOT_SUPPORTED_ERR.

* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-184E7107
*/
function nodeappendchild01() {
   var success;
    if(checkInitialization(builder, "nodeappendchild01") != null) return;
    var doc;
      var domImpl;
      var docType;
      var nullPubId = null;

      var nullSysId = null;

      var appendedChild;
      var tagName;
      var docElem;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      docElem = doc.documentElement;

      tagName = docElem.tagName;

      domImpl = doc.implementation;
docType = domImpl.createDocumentType(tagName,nullPubId,nullSysId);
      
      try {
      appendedChild = doc.appendChild(docType);
      fail("throw_HIERARCHY_REQUEST_OR_NOT_SUPPORTED");
     
      } catch (ex) {
		  if (typeof(ex.code) != 'undefined') {      
       switch(ex.code) {
       case /* HIERARCHY_REQUEST_ERR */ 3 :
       break;
      case /* NOT_SUPPORTED_ERR */ 9 :
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
   nodeappendchild01();
}
