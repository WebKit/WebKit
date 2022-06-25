
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level1/core/hc_nodeappendchilddocfragment";
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
    If the "newChild" is a DocumentFragment object then
    all its content is added to the child list of this node.
    
    Create and populate a new DocumentFragment object and
    append it to the second employee.   After the 
    "appendChild(newChild)" method is invoked retrieve the
    new nodes at the end of the list, they should be the
    two Element nodes from the DocumentFragment.

* @author Curt Arnold
* @see http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-184E7107
* @see http://www.w3.org/Bugs/Public/show_bug.cgi?id=246
* @see http://www.w3.org/Bugs/Public/show_bug.cgi?id=247
*/
function hc_nodeappendchilddocfragment() {
   var success;
    if(checkInitialization(builder, "hc_nodeappendchilddocfragment") != null) return;
    var doc;
      var elementList;
      var employeeNode;
      var childList;
      var newdocFragment;
      var newChild1;
      var newChild2;
      var child;
      var childName;
      var result = new Array();

      var appendedChild;
      var nodeType;
      expected = new Array();
      expected[0] = "em";
      expected[1] = "strong";
      expected[2] = "code";
      expected[3] = "sup";
      expected[4] = "var";
      expected[5] = "acronym";
      expected[6] = "br";
      expected[7] = "b";

      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      elementList = doc.getElementsByTagName("p");
      employeeNode = elementList.item(1);
      childList = employeeNode.childNodes;

      newdocFragment = doc.createDocumentFragment();
      newChild1 = doc.createElement("br");
      newChild2 = doc.createElement("b");
      appendedChild = newdocFragment.appendChild(newChild1);
      appendedChild = newdocFragment.appendChild(newChild2);
      appendedChild = employeeNode.appendChild(newdocFragment);
      for(var indexN100A2 = 0;indexN100A2 < childList.length; indexN100A2++) {
      child = childList.item(indexN100A2);
      nodeType = child.nodeType;

      
	if(
	(1 == nodeType)
	) {
	childName = child.nodeName;

      result[result.length] = childName;

	}
	
	}
   assertEqualsListAutoCase("element", "nodeNames",expected,result);
       
}




function runTest() {
   hc_nodeappendchilddocfragment();
}
