
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodesetuserdata08";
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
	Invoke setUserData on a CDATASection and EntityReference node to set their 
	UserData to this Document and DocumentElement node.  Verify if the UserData 
	object that was set for both nodes is different.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Node3-setUserData
*/
function nodesetuserdata08() {
   var success;
    if(checkInitialization(builder, "nodesetuserdata08") != null) return;
    var doc;
      var docElem;
      var entRef;
      var cData;
      var elemList;
      var elemName;
      var userData;
      var returned1;
      var returned2;
      var success;
      var retUserData;
      var nullHandler = null;

      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docElem = doc.documentElement;

      entRef = doc.createEntityReference("delta");
      cData = doc.createCDATASection("CDATASection");
      if (null == nullHandler) {
         entRef.setUserData("Key1", doc, null);
      } else {
          entRef.setUserData("Key1", doc, nullHandler.handle);
      }
       if (null == nullHandler) {
         cData.setUserData("Key2", docElem, null);
      } else {
          cData.setUserData("Key2", docElem, nullHandler.handle);
      }
       returned1 = entRef.getUserData("Key1");
      returned2 = cData.getUserData("Key2");
      success = returned1.isEqualNode(returned2);
      assertFalse("nodesetuserdata08",success);

}




function runTest() {
   nodesetuserdata08();
}
