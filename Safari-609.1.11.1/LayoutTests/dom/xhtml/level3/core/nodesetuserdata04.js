
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodesetuserdata04";
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

	
	Invoke setUserData on a new Element to set its UserData to a new Text node
	twice using different Keys.  Using getUserData with each Key and isNodeEqual 
	verify if the returned nodes are Equal.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Node3-setUserData
*/
function nodesetuserdata04() {
   var success;
    if(checkInitialization(builder, "nodesetuserdata04") != null) return;
    var doc;
      var userData;
      var returned1;
      var returned2;
      var retUserData;
      var success;
      var elem;
      var txt;
      var nullHandler = null;

      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      elem = doc.createElementNS("http://www.w3.org/1999/xhtml","p");
      txt = doc.createTextNode("TEXT");
      if (null == nullHandler) {
         elem.setUserData("Key1", txt, null);
      } else {
          elem.setUserData("Key1", txt, nullHandler.handle);
      }
       if (null == nullHandler) {
         elem.setUserData("Key2", txt, null);
      } else {
          elem.setUserData("Key2", txt, nullHandler.handle);
      }
       returned1 = elem.getUserData("Key1");
      returned2 = elem.getUserData("Key2");
      success = returned1.isEqualNode(returned2);
      assertTrue("nodesetuserdata04",success);

}




function runTest() {
   nodesetuserdata04();
}
