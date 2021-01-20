
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentrenamenode10";
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
       setImplementationAttribute("namespaceAware", true);

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
	The method renameNode renames an existing node and raises a  NAMESPACE_ERR
	if the qualifiedName has a prefix and the namespaceURI is null but a 
	NOT_SUPPORTED_ERR should be raised since the the type of the specified node is 
	neither ELEMENT_NODE nor ATTRIBUTE_NODE.
 
	Invoke the renameNode method on a new document node to rename a node to nodes 
	with malformed qualifiedNames.
	Check if a NOT_SUPPORTED_ERR gets thrown instead of a NAMESPACE_ERR.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-renameNode
*/
function documentrenamenode10() {
   var success;
    if(checkInitialization(builder, "documentrenamenode10") != null) return;
    var doc;
      var textEntry = "hello";
      var textNode;
      var renamedNode;
      var qualifiedName;
      var nullDocType = null;

      qualifiedNames = new Array();
      qualifiedNames[0] = "_:";
      qualifiedNames[1] = ":0";
      qualifiedNames[2] = ":";
      qualifiedNames[3] = "a0:0";
      qualifiedNames[4] = "_:0;";
      qualifiedNames[5] = "a:::::c";

      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      textNode = doc.createTextNode(textEntry);
      for(var indexN10060 = 0;indexN10060 < qualifiedNames.length; indexN10060++) {
      qualifiedName = qualifiedNames[indexN10060];
      
	{
		success = false;
		try {
            renamedNode = doc.renameNode(textNode,"http://www.w3.org/XML/1998/namespace",qualifiedName);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 9);
		}
		assertTrue("documentrenamenode10_NOT_SUPPORTED_ERR",success);
	}

	}
   
}




function runTest() {
   documentrenamenode10();
}
