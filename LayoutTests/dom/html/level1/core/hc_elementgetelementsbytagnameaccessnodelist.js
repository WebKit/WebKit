
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level1/core/hc_elementgetelementsbytagnameaccessnodelist";
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
The "getElementsByTagName(name)" method returns a list
of all descendant Elements in the order the children
were encountered in a pre order traversal of the element
tree.

Create a NodeList of all the descendant elements
using the string "p" as the tagName.
The method should return a NodeList whose length is
"5" in the order the children were encountered.
Access the FOURTH element in the NodeList.  The FOURTH
element, the first or second should be an "em" node with
the content "EMP0004".

* @author Curt Arnold
* @see http://www.w3.org/TR/1998/REC-DOM-Level-1-19981001/level-one-core#ID-1938918D
* @see http://www.w3.org/Bugs/Public/show_bug.cgi?id=246
*/
function hc_elementgetelementsbytagnameaccessnodelist() {
   var success;
    if(checkInitialization(builder, "hc_elementgetelementsbytagnameaccessnodelist") != null) return;
    var doc;
      var elementList;
      var testEmployee;
      var firstC;
      var childName;
      var nodeType;
      var employeeIDNode;
      var employeeID;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      elementList = doc.getElementsByTagName("p");
      testEmployee = elementList.item(3);
      firstC = testEmployee.firstChild;

      nodeType = firstC.nodeType;

      
    while(
	(3 == nodeType)
	) {
	firstC = firstC.nextSibling;

      nodeType = firstC.nodeType;

      
	}
childName = firstC.nodeName;

      assertEqualsAutoCase("element", "childName","em",childName);
       employeeIDNode = firstC.firstChild;

      employeeID = employeeIDNode.nodeValue;

      assertEquals("employeeID","EMP0004",employeeID);
       
}




function runTest() {
   hc_elementgetelementsbytagnameaccessnodelist();
}
