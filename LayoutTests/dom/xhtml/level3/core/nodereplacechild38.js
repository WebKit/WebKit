
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodereplacechild38";
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
	Using replaceChild on an Entity node attempt to replace its Text child with new Text,
	Comment, ProcessingInstruction and CDATASection nodes and in each case verify if 
	a NO_MODIFICATION_ALLOWED_ERR is raised.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-785887307
*/
function nodereplacechild38() {
   var success;
    if(checkInitialization(builder, "nodereplacechild38") != null) return;
    var doc;
      var docType;
      var entitiesMap;
      var ent;
      var oldChild;
      var entRef;
      var txt;
      var elem;
      var comment;
      var pi;
      var cdata;
      var replaced;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docType = doc.doctype;

      entitiesMap = docType.entities;

      ent = entitiesMap.getNamedItem("alpha");
      assertNotNull("alphaEntity",ent);
oldChild = ent.firstChild;

      assertNotNull("alphaText",oldChild);
cdata = doc.createCDATASection("CDATASection");
      
	{
		success = false;
		try {
            replaced = ent.replaceChild(cdata,oldChild);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 7);
		}
		assertTrue("throw_NO_MODIFICATION_ALLOWED_ERR1",success);
	}
pi = doc.createProcessingInstruction("target","data");
      
	{
		success = false;
		try {
            replaced = ent.replaceChild(pi,oldChild);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 7);
		}
		assertTrue("throw_NO_MODIFICATION_ALLOWED_ERR2",success);
	}
comment = doc.createComment("Comment");
      
	{
		success = false;
		try {
            replaced = ent.replaceChild(comment,oldChild);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 7);
		}
		assertTrue("throw_NO_MODIFICATION_ALLOWED_ERR3",success);
	}
txt = doc.createTextNode("Text");
      
	{
		success = false;
		try {
            replaced = ent.replaceChild(txt,oldChild);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 7);
		}
		assertTrue("throw_NO_MODIFICATION_ALLOWED_ERR4",success);
	}
elem = doc.createElementNS("http://www.w3.org/1999/xhtml","xhtml:p");
      
	{
		success = false;
		try {
            replaced = ent.replaceChild(elem,oldChild);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 7);
		}
		assertTrue("throw_NO_MODIFICATION_ALLOWED_ERR5",success);
	}
entRef = doc.createEntityReference("delta");
      
	{
		success = false;
		try {
            replaced = ent.replaceChild(entRef,oldChild);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 7);
		}
		assertTrue("throw_NO_MODIFICATION_ALLOWED_ERR6",success);
	}

}




function runTest() {
   nodereplacechild38();
}
