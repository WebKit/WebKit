
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentrenamenode09";
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
	The method renameNode renames an existing node. When the specified node was created 
	from a different document than this document, a WRONG_DOCUMENT_ERR exception is thrown.
 
	Invoke the renameNode method on a new Document node to rename a new attribute node
	created in the original Document, but later adopted by this new document node.  The 
	ownerDocument attribute of this attribute has now changed, such that the attribute node is considered to 
        be created from this new document node. Verify that no exception is thrown upon renaming and verify
        the new nodeName of this attribute node. 

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-renameNode
*/
function documentrenamenode09() {
   var success;
    if(checkInitialization(builder, "documentrenamenode09") != null) return;
    var doc;
      var newDoc;
      var domImpl;
      var attr;
      var renamedNode;
      var adopted;
      var nullDocType = null;

      var attrNodeName;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      domImpl = doc.implementation;
newDoc = domImpl.createDocument("http://www.w3.org/DOM/Test","dom:newD",nullDocType);
      attr = doc.createAttributeNS("http://www.w3.org/DOM/Test","test");
      adopted = newDoc.adoptNode(attr);
      renamedNode = newDoc.renameNode(attr,"http://www.w3.org/2000/xmlns/","xmlns:xmlns");
      attrNodeName = renamedNode.nodeName;

      assertEquals("documentrenamenode09_1","xmlns:xmlns",attrNodeName);
       
}




function runTest() {
   documentrenamenode09();
}
