
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/attrisid07";
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
	The method isId returns whether this attribute is known to be of type ID or not.
	
	Add a new attribute of type ID to the third acronym element node of this document. Verify that the method
        isId returns true. The use of Element.setIdAttributeNS() makes 'isId' a user-determined ID attribute.
	Import the newly created attribute node into this document.  
        Since user data assocated to the imported node is not carried over, verify that the method isId
        returns false on the imported attribute node.        


* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Attr-isId
*/
function attrisid07() {
   var success;
    if(checkInitialization(builder, "attrisid07") != null) return;
    var doc;
      var elemList;
      var acronymElem;
      var attributesMap;
      var attr;
      var attrImported;
      var id = false;
      var elem;
      var elemName;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      elemList = doc.getElementsByTagNameNS("*","acronym");
      acronymElem = elemList.item(2);
      acronymElem.setAttributeNS("http://www.w3.org/DOM","dom3:newAttr","null");
      acronymElem.setIdAttributeNS("http://www.w3.org/DOM","newAttr",true);
      attr = acronymElem.getAttributeNodeNS("http://www.w3.org/DOM","newAttr");
      id = attr.isId;

      assertTrue("AttrIsIDTrue07_1",id);
attrImported = doc.importNode(attr,false);
      id = attrImported.isId;

      assertFalse("AttrIsID07_isFalseforImportedNode",id);

}




function runTest() {
   attrisid07();
}
