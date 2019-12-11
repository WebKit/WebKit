
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level2/html/HTMLOptionsCollection06";
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
      docsLoaded += preload(docRef, "doc", "optionscollection");
        
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
    An HTMLOptionsCollection is a list of nodes representing HTML option
    element.
    An individual node may be accessed by either ordinal index, the node's
    name or id attributes.  (Test ordinal index).
    The item() method retrieves a node specified by ordinal index.  
    A value of null is returned if the index is out of range.

    Retrieve the first SELECT element.  Create a HTMLOptionsCollection.
    Retrieve the tenth item in the list - null should be returned since
    there are not 10 items in the list.

* @author NIST
* @author Rick Rivello
* @see http://www.w3.org/TR/DOM-Level-2-HTML/html#HTMLOptionsCollection-item
*/
function HTMLOptionsCollection06() {
   var success;
    if(checkInitialization(builder, "HTMLOptionsCollection06") != null) return;
    var nodeList;
      var testNode;
      var optionsNode;
      var optionsValueNode;
      var optionsList;
      var vvalue;
      var doc;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "optionscollection");
      nodeList = doc.getElementsByTagName("select");
      assertSize("Asize",2,nodeList);
testNode = nodeList.item(0);
      optionsList = testNode.options;

      optionsNode = optionsList.item(10);
      assertNull("optionsIndexLink",optionsNode);
    
}




function runTest() {
   HTMLOptionsCollection06();
}
