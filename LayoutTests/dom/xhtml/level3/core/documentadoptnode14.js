
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentadoptnode14";
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
	Using the method adoptNode in a new Document, adopt a newly created DocumentFragment node populated with
	with the first acronym element of this Document as its newChild.  Since the decendants of a documentFragment
	are recursively adopted, check if the adopted node has children.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-adoptNode
*/
function documentadoptnode14() {
   var success;
    if(checkInitialization(builder, "documentadoptnode14") != null) return;
    var doc;
      var newDoc;
      var docElem;
      var domImpl;
      var docFragment;
      var childList;
      var success;
      var acronymNode;
      var adoptedDocFrag;
      var appendedChild;
      var nullDocType = null;

      var imported;
      var rootNS;
      var rootName;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docElem = doc.documentElement;

      rootName = docElem.tagName;

      rootNS = docElem.namespaceURI;

      domImpl = doc.implementation;
newDoc = domImpl.createDocument(rootNS,rootName,nullDocType);
      docFragment = newDoc.createDocumentFragment();
      imported = newDoc.importNode(docElem,true);
      docElem = newDoc.documentElement;

      appendedChild = docElem.appendChild(imported);
      childList = newDoc.getElementsByTagName("acronym");
      acronymNode = childList.item(0);
      appendedChild = docFragment.appendChild(acronymNode);
      adoptedDocFrag = newDoc.adoptNode(docFragment);
      
	if(
	
	(adoptedDocFrag != null)

	) {
	success = adoptedDocFrag.hasChildNodes();
      assertTrue("documentadoptnode14",success);

	}
	
}




function runTest() {
   documentadoptnode14();
}
