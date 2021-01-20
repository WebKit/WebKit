
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodeisequalnode11";
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
        
      var dupDocRef = null;
      if (typeof(this.dupDoc) != 'undefined') {
        dupDocRef = this.dupDoc;
      }
      docsLoaded += preload(dupDocRef, "dupDoc", "hc_staff");
        
       if (docsLoaded == 2) {
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
    if (++docsLoaded == 2) {
        setUpPageStatus = 'complete';
    }
}


/**
* 
	Retreive the first element node whose localName is "p".  Import it into a new
	Document with deep=false.  Using isEqualNode check if the original and the imported
	Element Node are not equal the child nodes are different.
	Import with deep and the should still be unequal if
	validating since the
	new document does not provide the same default attributes.
	Import it into another instance of the source document
	and then the imported node and the source should be equal.   

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Node3-isEqualNode
* @see http://www.w3.org/Bugs/Public/show_bug.cgi?id=529
*/
function nodeisequalnode11() {
   var success;
    if(checkInitialization(builder, "nodeisequalnode11") != null) return;
    var doc;
      var domImpl;
      var employeeList;
      var newDoc;
      var dupDoc;
      var elem1;
      var elem2;
      var elem3;
      var elem4;
      var isEqual;
      var nullDocType = null;

      var docElem;
      var rootNS;
      var rootName;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docElem = doc.documentElement;

      rootNS = docElem.namespaceURI;

      rootName = docElem.tagName;

      domImpl = doc.implementation;
newDoc = domImpl.createDocument(rootNS,rootName,nullDocType);
      employeeList = doc.getElementsByTagName("p");
      elem1 = employeeList.item(0);
      elem2 = newDoc.importNode(elem1,false);
      isEqual = elem1.isEqualNode(elem2);
      assertFalse("nodeisequalnodeFalse11",isEqual);
elem3 = newDoc.importNode(elem1,true);
      isEqual = elem1.isEqualNode(elem3);
      
	if(
	(getImplementationAttribute("validating") == true)
	) {
	assertFalse("deepImportNoDTD",isEqual);

	}
	
      var dupDocRef = null;
      if (typeof(this.dupDoc) != 'undefined') {
        dupDocRef = this.dupDoc;
      }
      dupDoc = load(dupDocRef, "dupDoc", "hc_staff");
      elem4 = dupDoc.importNode(elem1,true);
      isEqual = elem1.isEqualNode(elem4);
      assertTrue("deepImportSameDTD",isEqual);

}




function runTest() {
   nodeisequalnode11();
}
