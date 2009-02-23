
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentadoptnode16";
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
Create a document fragment with an entity reference, adopt the node and check
that the entity reference value comes from the adopting documents DTD.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-adoptNode
*/
function documentadoptnode16() {
   var success;
    if(checkInitialization(builder, "documentadoptnode16") != null) return;
    var doc;
      var docFragment;
      var childList;
      var parent;
      var child;
      var childsAttr;
      var entRef;
      var textNode;
      var adopted;
      var parentImp;
      var childImp;
      var attributes;
      var childAttrImp;
      var nodeValue;
      var appendedChild;
      var attrNode;
      var firstChild;
      var firstChildType;
      var firstChildName;
      var firstChildValue;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docFragment = doc.createDocumentFragment();
      parent = doc.createElement("parent");
      child = doc.createElement("child");
      childsAttr = doc.createAttribute("state");
      entRef = doc.createEntityReference("gamma");
      textNode = doc.createTextNode("Test");
      appendedChild = childsAttr.appendChild(entRef);
      attrNode = child.setAttributeNode(childsAttr);
      appendedChild = child.appendChild(textNode);
      appendedChild = parent.appendChild(child);
      appendedChild = docFragment.appendChild(parent);
      adopted = doc.adoptNode(docFragment);
      
	if(
	
	(adopted != null)

	) {
	parentImp = adopted.firstChild;

      childImp = parentImp.firstChild;

      attributes = childImp.attributes;

      childAttrImp = attributes.getNamedItem("state");
      firstChild = childAttrImp.firstChild;

      assertNotNull("firstChildNotNull",firstChild);
firstChildName = firstChild.nodeName;

      firstChildValue = firstChild.nodeValue;

      firstChildType = firstChild.nodeType;

      
	if(
	(5 == firstChildType)
	) {
	assertEquals("firstChildEnt3Ref","gamma",firstChildName);
       
	}
	
		else {
			assertEquals("documentadoptnode16","Texas",firstChildValue);
       
		}
	
	}
	
}




function runTest() {
   documentadoptnode16();
}
