
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level2/events/initEvent04";
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
       checkFeature("MutationEvents", "2.0");

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
The Event.initEvent method is called for event returned by 
DocumentEvent.createEvent("MutationEvents")
and the state is checked to see if it reflects the parameters.

* @author Curt Arnold
* @see http://www.w3.org/TR/DOM-Level-2-Events/events#Events-Event-initEvent
*/
function initEvent04() {
   var success;
    if(checkInitialization(builder, "initEvent04") != null) return;
    var doc;
      var event;
      var expectedEventType = "rotate";
      var actualEventType;
      var expectedCanBubble = true;
      var actualCanBubble;
      var expectedCancelable = false;
      var actualCancelable;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      event = doc.createEvent("MutationEvents");
      assertNotNull("notnull",event);
event.initEvent(expectedEventType,expectedCanBubble,expectedCancelable);
      actualEventType = event.type;

      assertEquals("type",expectedEventType,actualEventType);
       actualCancelable = event.cancelable;

      assertEquals("cancelable",expectedCancelable,actualCancelable);
       actualCanBubble = event.bubbles;

      assertEquals("canBubble",expectedCanBubble,actualCanBubble);
       
}




function runTest() {
   initEvent04();
}
