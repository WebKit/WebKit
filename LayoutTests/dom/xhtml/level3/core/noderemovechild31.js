
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/noderemovechild31";
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
       setImplementationAttribute("expandEntityReferences", false);

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
	Using removeChild on a default Attribute node attempt to remove its EntityReference child node and
	and verify the name of the returned node that was removed.  Now attempt the reverse
	and verify if a NO_MODIFICATION_ALLOWED_ERR or NOT_FOUND_ERR is thrown.
	Then remove an child of the entity reference and expect a NO_MODIFICATION_ALLOWED_ERR.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-1734834066
*/
function noderemovechild31() {
   var success;
    if(checkInitialization(builder, "noderemovechild31") != null) return;
    var doc;
      var parentList;
      var attrsMap;
      var parent;
      var child;
      var entRef;
      var elem;
      var removed;
      var removedNode;
      var removedName;
      var appendedChild;
      var entRefChild;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      parentList = doc.getElementsByTagName("acronym");
      elem = parentList.item(3);
      attrsMap = elem.attributes;

      parent = attrsMap.getNamedItem("class");
      entRef = doc.createEntityReference("delta");
      appendedChild = parent.appendChild(entRef);
      child = parent.lastChild;

      removed = parent.removeChild(child);
      removedName = removed.nodeName;

      assertEquals("noderemovechild31","delta",removedName);
       
      try {
      removedNode = child.removeChild(parent);
      fail("throw_DOMException");
     
      } catch (ex) {
		  if (typeof(ex.code) != 'undefined') {      
       switch(ex.code) {
       case /* NO_MODIFICATION_ALLOWED_ERR */ 7 :
       break;
      case /* NOT_FOUND_ERR */ 8 :
       break;
          default:
          throw ex;
          }
       } else { 
       throw ex;
        }
         }
        entRefChild = child.firstChild;

      
	if(
	
	(entRefChild != null)

	) {
	
	{
		success = false;
		try {
            removedNode = child.removeChild(entRefChild);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 7);
		}
		assertTrue("throw_NO_MODIFICATION_ALLOWED_ERR",success);
	}

	}
	
}




function runTest() {
   noderemovechild31();
}
