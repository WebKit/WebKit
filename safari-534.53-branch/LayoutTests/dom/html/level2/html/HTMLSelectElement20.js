
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level2/html/HTMLSelectElement20";
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
      docsLoaded += preload(docRef, "doc", "select");
        
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
Attempting to add an new option using HTMLSelectElement.add before a node that is not a child of the select
element should raise a NOT_FOUND_ERR.

* @author Curt Arnold
* @see http://www.w3.org/TR/DOM-Level-2-HTML/html#ID-14493106
*/
function HTMLSelectElement20() {
   var success;
    if(checkInitialization(builder, "HTMLSelectElement20") != null) return;
    var nodeList;
      var testNode;
      var doc;
      var optLength;
      var selected;
      var newOpt;
      var newOptText;
      var retNode;
      var options;
      var otherSelect;
      var selectedNode;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "select");
      nodeList = doc.getElementsByTagName("select");
      assertSize("Asize",3,nodeList);
testNode = nodeList.item(0);
      otherSelect = nodeList.item(1);
      newOpt = doc.createElement("option");
      newOptText = doc.createTextNode("EMP31415");
      retNode = newOpt.appendChild(newOptText);
      options = otherSelect.options;

      selectedNode = options.item(0);
      
	{
		success = false;
		try {
            testNode.add(newOpt,selectedNode);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 8);
		}
		assertTrue("throw_NOT_FOUND_ERR",success);
	}

}




function runTest() {
   HTMLSelectElement20();
}
