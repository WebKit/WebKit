
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentadoptnode21";
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
	
	Invoke the adoptNode method on this Document with the source node being an existing attribute
        that is a part of this Document.   Verify that the returned adopted node's nodeName, nodeValue
        and nodeType are as expected and that the ownerElement attribute of the returned attribute node 
        was set to null.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-adoptNode
*/
function documentadoptnode21() {
   var success;
    if(checkInitialization(builder, "documentadoptnode21") != null) return;
    var doc;
      var attrOwnerElem;
      var element;
      var attr;
      var childList;
      var adoptedTitle;
      var attrsParent;
      var nodeName;
      var nodeType;
      var nodeValue;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      childList = doc.getElementsByTagName("acronym");
      element = childList.item(0);
      attr = element.getAttributeNode("title");
      adoptedTitle = doc.adoptNode(attr);
      nodeName = adoptedTitle.nodeName;

      nodeValue = adoptedTitle.nodeValue;

      nodeType = adoptedTitle.nodeType;

      attrOwnerElem = adoptedTitle.ownerElement;

      assertEquals("documentadoptnode21_nodeName","title",nodeName);
       assertEquals("documentadoptnode21_nodeType",2,nodeType);
       assertEquals("documentadoptnode21_nodeValue","Yes",nodeValue);
       assertNull("documentadoptnode21_ownerDoc",attrOwnerElem);
    
}




function runTest() {
   documentadoptnode21();
}
