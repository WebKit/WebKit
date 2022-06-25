
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodeinsertbefore13";
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
        
      var docAltRef = null;
      if (typeof(this.docAlt) != 'undefined') {
        docAltRef = this.docAlt;
      }
      docsLoaded += preload(docAltRef, "docAlt", "hc_staff");
        
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



	Using insertBefore on a DocumentFragment node attempt to insert a new Element node 
	created by another Document, before this DocumentFragment's Element node and 
	verify if a WRONG_DOCUMENT_ERR is raised. 

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-952280727
*/
function nodeinsertbefore13() {
   var success;
    if(checkInitialization(builder, "nodeinsertbefore13") != null) return;
    var doc;
      var docAlt;
      var docFrag;
      var elemAlt;
      var elem;
      var appendedChild;
      var inserted;
      var docElem;
      var rootNS;
      var rootTagname;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docElem = doc.documentElement;

      rootNS = docElem.namespaceURI;

      rootTagname = docElem.tagName;

      
      var docAltRef = null;
      if (typeof(this.docAlt) != 'undefined') {
        docAltRef = this.docAlt;
      }
      docAlt = load(docAltRef, "docAlt", "hc_staff");
      docFrag = doc.createDocumentFragment();
      elem = doc.createElementNS(rootNS,rootTagname);
      elemAlt = docAlt.createElementNS(rootNS,rootTagname);
      appendedChild = docFrag.appendChild(elem);
      
	{
		success = false;
		try {
            inserted = docFrag.insertBefore(elemAlt,elem);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 4);
		}
		assertTrue("throw_WRONG_DOCUMENT_ERR",success);
	}

}




function runTest() {
   nodeinsertbefore13();
}
