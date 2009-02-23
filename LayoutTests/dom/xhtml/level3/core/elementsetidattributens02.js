
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/elementsetidattributens02";
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
     Invoke setIdAttributeNS on an existing attribute with a namespace URI and a qualified name.  Verify by calling
     isID on the attribute node and getElementById on document node. Assume the grammar has not defined any
     element of typeID. Call setIdAttributeNS with isId=false to reset. Method isId should now return false. 
    
* @author IBM
* @author Jenny Hsu
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-ElSetIdAttrNS
*/
function elementsetidattributens02() {
   var success;
    if(checkInitialization(builder, "elementsetidattributens02") != null) return;
    var doc;
      var elemList;
      var addressElem;
      var attributesMap;
      var attr;
      var id = false;
      var elem;
      var elemName;
      var xsiNS = "http://www.w3.org/2001/XMLSchema-instance";
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      elemList = doc.getElementsByTagNameNS("*","acronym");
      addressElem = elemList.item(2);
      addressElem.setIdAttributeNS(xsiNS,"noNamespaceSchemaLocation",true);
      attributesMap = addressElem.attributes;

      attr = attributesMap.getNamedItem("xsi:noNamespaceSchemaLocation");
      id = attr.isId;

      assertTrue("elementsetidattributensIsIdTrue02",id);
elem = doc.getElementById("Yes");
      assertNotNull("getElementByIDNotNull",elem);
elemName = elem.tagName;

      assertEquals("elementsetidattributensGetElementById01","acronym",elemName);
       addressElem.setIdAttributeNS(xsiNS,"noNamespaceSchemaLocation",false);
      id = attr.isId;

      assertFalse("elementsetidattributensIsIdFalse02",id);

}




function runTest() {
   elementsetidattributens02();
}
