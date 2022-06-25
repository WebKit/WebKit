
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level2/html/HTMLTableSectionElement28";
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
      docsLoaded += preload(docRef, "doc", "tablesection");
        
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
    The deleteRow() method throws a INDEX_SIZE_ERR DOMException
    if the specified index is equal to the number of rows.
 
    Retrieve the first THEAD element which has one row.  Try
    to delete a row using an index of 1.  This should throw
    a INDEX_SIZE_ERR DOMException since the index is equal to the
    number of rows.

* @author NIST
* @author Rick Rivello
* @see http://www.w3.org/TR/DOM-Level-2-HTML/html#ID-5625626
* @see http://www.w3.org/TR/DOM-Level-2-HTML/html#xpointer(id('ID-5625626')/raises/exception[@name='DOMException']/descr/p[substring-before(.,':')='INDEX_SIZE_ERR'])
*/
function HTMLTableSectionElement28() {
   var success;
    if(checkInitialization(builder, "HTMLTableSectionElement28") != null) return;
    var nodeList;
      var testNode;
      var doc;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "tablesection");
      nodeList = doc.getElementsByTagName("thead");
      assertSize("Asize",1,nodeList);
testNode = nodeList.item(0);
      
	{
		success = false;
		try {
            testNode.deleteRow(1);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 1);
		}
		assertTrue("HTMLTableSectionElement28",success);
	}

}




function runTest() {
   HTMLTableSectionElement28();
}
