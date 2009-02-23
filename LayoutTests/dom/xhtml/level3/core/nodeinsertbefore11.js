
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodeinsertbefore11";
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



	Using insertBefore on a DocumentFragment node attempt to insert a child nodes before
	other permissible nodes and verify the contents/name of each inserted node.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-952280727
*/
function nodeinsertbefore11() {
   var success;
    if(checkInitialization(builder, "nodeinsertbefore11") != null) return;
    var doc;
      var docFrag;
      var elem;
      var pi;
      var comment;
      var txt;
      var cdata;
      var eRef;
      var inserted;
      var insertedVal;
      var appendedChild;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docFrag = doc.createDocumentFragment();
      elem = doc.createElementNS("http://www.w3.org/1999/xhtml","body");
      pi = doc.createProcessingInstruction("PITarget","PIData");
      comment = doc.createComment("Comment");
      txt = doc.createTextNode("Text");
      cdata = doc.createCDATASection("CDATA");
      eRef = doc.createEntityReference("alpha");
      appendedChild = docFrag.appendChild(elem);
      appendedChild = docFrag.appendChild(pi);
      appendedChild = docFrag.appendChild(comment);
      appendedChild = docFrag.appendChild(txt);
      appendedChild = docFrag.appendChild(cdata);
      appendedChild = docFrag.appendChild(eRef);
      inserted = docFrag.insertBefore(comment,pi);
      insertedVal = inserted.data;

      assertEquals("nodeinsertbefore11_Comment","Comment",insertedVal);
       inserted = docFrag.insertBefore(txt,comment);
      insertedVal = inserted.data;

      assertEquals("nodeinsertbefore11_Text","Text",insertedVal);
       inserted = docFrag.insertBefore(cdata,txt);
      insertedVal = inserted.data;

      assertEquals("nodeinsertbefore11_CDATA","CDATA",insertedVal);
       inserted = docFrag.insertBefore(eRef,cdata);
      insertedVal = inserted.nodeName;

      assertEquals("nodeinsertbefore11_Ent1","alpha",insertedVal);
       
}




function runTest() {
   nodeinsertbefore11();
}
