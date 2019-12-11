
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/elementsetidattributens12";
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
     Declares the attribute specified by local name and namespace URI to be of type ID. If the value of the 
     specified attribute is unique then this element node can later be retrieved using getElementById on Document. 
     Note, however, that this simply affects this node and does not change any grammar that may be in use. 
     
     Set the noNamespaceSchemaLocation attribute on the first acronym element to "No".  Invoke setIdAttributeNS on the 
     noNamespaceSchemaLocation attribute of the first, second and third acronym element.  Verify by calling isId on 
     the attributes.  Calling getElementById with "No" as a value should return the acronym element.  
    
* @author IBM
* @author Jenny Hsu
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-ElSetIdAttrNS
*/
function elementsetidattributens12() {
   var success;
    if(checkInitialization(builder, "elementsetidattributens12") != null) return;
    var doc;
      var elemList;
      var acronymElem1;
      var acronymElem2;
      var acronymElem3;
      var attributesMap;
      var attr;
      var id = false;
      var elem;
      var elemName;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      elemList = doc.getElementsByTagNameNS("*","acronym");
      acronymElem1 = elemList.item(0);
      acronymElem2 = elemList.item(1);
      acronymElem3 = elemList.item(2);
      acronymElem1.setAttributeNS("http://www.w3.org/2001/XMLSchema-instance","xsi:noNamespaceSchemaLocation","No");
      acronymElem1.setIdAttributeNS("http://www.w3.org/2001/XMLSchema-instance","noNamespaceSchemaLocation",true);
      acronymElem2.setIdAttributeNS("http://www.w3.org/2001/XMLSchema-instance","noNamespaceSchemaLocation",true);
      acronymElem3.setIdAttributeNS("http://www.w3.org/2001/XMLSchema-instance","noNamespaceSchemaLocation",true);
      attributesMap = acronymElem1.attributes;

      attr = attributesMap.getNamedItem("xsi:noNamespaceSchemaLocation");
      id = attr.isId;

      assertTrue("elementsetidattributensIsId1True12",id);
attributesMap = acronymElem2.attributes;

      attr = attributesMap.getNamedItem("xsi:noNamespaceSchemaLocation");
      id = attr.isId;

      assertTrue("elementsetidattributensIsId2True12",id);
attributesMap = acronymElem3.attributes;

      attr = attributesMap.getNamedItem("xsi:noNamespaceSchemaLocation");
      id = attr.isId;

      assertTrue("elementsetidattributensIsId3True12",id);
elem = doc.getElementById("No");
      elemName = elem.tagName;

      assertEquals("elementsetidattributensGetElementById10","acronym",elemName);
       
}




function runTest() {
   elementsetidattributens12();
}
