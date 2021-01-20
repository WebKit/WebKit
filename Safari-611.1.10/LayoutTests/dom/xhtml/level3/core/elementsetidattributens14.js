
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/elementsetidattributens14";
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
     
     Invoke setIdAttributeNS on two existing attributes of the second p element and the third
     acronym element.  Verify by calling isId on the attributes and getElementById with different values on document node.  
    
* @author IBM
* @author Jenny Hsu
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-ElSetIdAttrNS
*/
function elementsetidattributens14() {
   var success;
    if(checkInitialization(builder, "elementsetidattributens14") != null) return;
    var doc;
      var elemList;
      var pElem;
      var acronymElem;
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
      elemList = doc.getElementsByTagNameNS("*","p");
      pElem = elemList.item(1);
      elemList = doc.getElementsByTagNameNS("*","acronym");
      acronymElem = elemList.item(2);
      pElem.setIdAttributeNS("http://www.w3.org/2000/xmlns/","dmstc",true);
      acronymElem.setIdAttributeNS("http://www.w3.org/2001/XMLSchema-instance","noNamespaceSchemaLocation",true);
      attributesMap = pElem.attributes;

      attr = attributesMap.getNamedItem("xmlns:dmstc");
      id = attr.isId;

      assertTrue("elementsetidattributensIsId1True14",id);
attributesMap = acronymElem.attributes;

      attr = attributesMap.getNamedItem("xsi:noNamespaceSchemaLocation");
      id = attr.isId;

      assertTrue("elementsetidattributensIsId2True14",id);
elem = doc.getElementById("Yes");
      elemName = elem.tagName;

      assertEquals("elementsetidattributens1GetElementById14","acronym",elemName);
       elem = doc.getElementById("http://www.usa.com");
      elemName = elem.tagName;

      assertEquals("elementsetidattributens2GetElementById14","p",elemName);
       
}




function runTest() {
   elementsetidattributens14();
}
