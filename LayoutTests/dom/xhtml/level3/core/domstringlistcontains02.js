
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/domstringlistcontains02";
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
	The contains method of the DOMStringList tests if a string is part of this DOMStringList.  
	
	Invoke the contains method on the list searching for several of the parameters recognized by the 
        DOMConfiguration object.  
	Verify that the list contains features that are required and supported by this DOMConfiguration object.
        Verify that the contains method returns false for a string that is not contained in this DOMStringList. 

* @author IBM
* @author Jenny Hsu
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#DOMStringList-contains
*/
function domstringlistcontains02() {
   var success;
    if(checkInitialization(builder, "domstringlistcontains02") != null) return;
    var doc;
      var paramList;
      var domConfig;
      var contain;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      domConfig = doc.domConfig;

      paramList = domConfig.parameterNames;

      contain = paramList.contains("comments");
      assertTrue("domstringlistcontains02_1",contain);
contain = paramList.contains("cdata-sections");
      assertTrue("domstringlistcontains02_2",contain);
contain = paramList.contains("entities");
      assertTrue("domstringlistcontains02_3",contain);
contain = paramList.contains("error-handler");
      assertTrue("domstringlistcontains02_4",contain);
contain = paramList.contains("infoset");
      assertTrue("domstringlistcontains02_5",contain);
contain = paramList.contains("namespace-declarations");
      assertTrue("domstringlistcontains02_6",contain);
contain = paramList.contains("element-content-whitespace");
      assertTrue("domstringlistcontains02_7",contain);
contain = paramList.contains("test");
      assertFalse("domstringlistcontains02_8",contain);

}




function runTest() {
   domstringlistcontains02();
}
