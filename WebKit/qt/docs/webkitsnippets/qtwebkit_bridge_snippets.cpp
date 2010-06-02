
void wrapInFunction()
{

//! [0]
    // ...
    QWebFrame *frame = myWebPage->mainFrame();
    frame->addToJavaScriptWindowObject("someNameForMyObject", myObject);
    // ...
//! [0]
#if 0
    //! [1]
   {
       width: ...,
       height: ...,
       toDataURL: function() { ... },
       assignToHTMLImageElement: function(element) { ... }
   }
   //! [1]
#endif
    //! [2]
     class MyObject : QObject {
         Q_OBJECT
         Q_PROPERTY(QPixmap myPixmap READ getPixmap)

     public:
         QPixmap getPixmap() const;
     };

     /* ... */

     MyObject myObject;
     myWebPage.mainFrame()->addToJavaScriptWindowObject("myObject", &myObject);

    //! [2]
 #if 0
     //! [3]
     <html>
        <head>
         <script>
             function loadImage() {
                 myObject.myPixmap.assignToHTMLImageElement(document.getElementById("imageElement"));
             }
         </script>
        </head>
        <body onload="loadImage()">
         <img id="imageElement" width="300" height="200" />
        </body>
     </html>
    //! [3]
 #endif
     //! [4]
      class MyObject : QObject {
          Q_OBJECT

      public slots:
          void doSomethingWithWebElement(const QWebElement&);
      };

      /* ... */

      MyObject myObject;
      myWebPage.mainFrame()->addToJavaScriptWindowObject("myObject", &myObject);

     //! [4]
  #if 0
      //! [5]
      <html>
         <head>
          <script>
              function runExample() {
                  myObject.doSomethingWithWebElement(document.getElementById("someElement"));
              }
          </script>
         </head>
         <body onload="runExample()">
          <span id="someElement">Text</span>
         </body>
      </html>
     //! [5]
  #endif
}

