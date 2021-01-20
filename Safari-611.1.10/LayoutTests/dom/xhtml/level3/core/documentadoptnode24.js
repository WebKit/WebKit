
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentadoptnode24";
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
	The adoptNode method changes the ownerDocument of a node, its children, as well as the 
	attached attribute nodes if there are any. If the node has a parent it is first removed 
	from its parent child list. 
	For Element Nodes, specified attribute nodes of the source element are adopted, Default 
	attributes are discarded and descendants of the source element are recursively adopted. 

	Invoke the adoptNode method on a new document with the first code element node of this
	Document as the source.  Verify if the node has been adopted correctly by checking the 
	length of the this elements childNode list before and after.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-adoptNode
*/
function documentadoptnode24() {
   var success;
    if(checkInitialization(builder, "documentadoptnode24") != null) return;
    var doc;
      var newDoc;
      var domImpl;
      var childList;
      var adoptedNode;
      var codeElem;
      var codeElemChildren;
      var adoptedChildren;
      var codeElemLen;
      var adoptedLen;
      var nullDocType = null;

      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      domImpl = doc.implementation;
newDoc = domImpl.createDocument("http://www.w3.org/DOM/Test","dom:test",nullDocType);
      childList = doc.getElementsByTagNameNS("*","code");
      codeElem = childList.item(0);
      adoptedNode = newDoc.adoptNode(codeElem);
      codeElemChildren = codeElem.childNodes;

      adoptedChildren = adoptedNode.childNodes;

      codeElemLen = codeElemChildren.length;

      adoptedLen = adoptedChildren.length;

      assertEquals("documentadoptnode24",adoptedLen,codeElemLen);
       
}




function runTest() {
   documentadoptnode24();
}
