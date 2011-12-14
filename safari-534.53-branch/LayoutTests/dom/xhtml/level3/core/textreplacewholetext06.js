
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/textreplacewholetext06";
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
       setImplementationAttribute("expandEntityReferences", true);

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
	The method replaceWholeText substitutes the a specified text for the text of 
	the current node and all logically-adjacent text nodes.  This method raises
	a NO_MODIFICATION_ALLOWED_ERR if one of the Text nodes being replaced is readonly.
	
	Invoke replaceWholeText on an existing text node with newly created text and Entityreference
	nodes (whose replacement text is a character entity reference) appended as children of its parent element node.  
	Where the nodes to be removed are read-only descendants of an EntityReference, the EntityReference 
        must be removed instead of the read-only nodes. Only if any EntityReference to be removed has 
        descendants that are not EntityReference, Text, or CDATASection nodes, the replaceWholeText 
        method must fail, raising a NO_MODIFICATION_ALLOWED_ERR. Verify that the method does not raise
        an exception and verify the content of the returned text node.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Text3-replaceWholeText
*/
function textreplacewholetext06() {
   var success;
    if(checkInitialization(builder, "textreplacewholetext06") != null) return;
    var doc;
      var itemList;
      var elementStrong;
      var textNode;
      var erefNode;
      var replacedText;
      var appendedChild;
      var nodeValue;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      itemList = doc.getElementsByTagName("strong");
      elementStrong = itemList.item(0);
      textNode = doc.createTextNode("New Text");
      erefNode = doc.createEntityReference("beta");
      appendedChild = elementStrong.appendChild(textNode);
      appendedChild = elementStrong.appendChild(erefNode);
      textNode = elementStrong.firstChild;

      replacedText = textNode.replaceWholeText("New Text and Cdata");
      nodeValue = textNode.nodeValue;

      assertEquals("textreplacewholetext06","New Text and Cdata",nodeValue);
       
}




function runTest() {
   textreplacewholetext06();
}
