#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <qrect.h>

int main() {

    QRect r0;
    QRect r1 = QRect(0,0,100,100);
    QRect r2 = QRect(100,100,200,200);
    QRect r3 = r1.unite(r2);
    QRect r4 = r3;
    QRect r5 = r3;
    QRect r6 = QRect(100,100,0,0); //purposely an invalid rect
    QRect r7;
    int x,y,w,h, xp1, xp2, yp1, yp2;
    
    cout << "r1: " << r1 << endl;
    cout << "r2: " << r2 << endl;
    cout << "r3 = r1.unite(r2): " << r3 << endl;
    cout << "r4 = r3: " << r4 << endl;
    cout << "r5 = r3: " << r5 << endl;
    cout << "r6: " << r6 << endl;
    
    r5.setLeft(10);
    cout << "r5 setLeft(10): " << r5 << endl;
    r5.setTop(10);
    cout << "r5 setTop(10): " << r5 << endl;
    r5.setRight(20);
    cout << "r5 setRight(20): " << r5 << endl;
    r5.setBottom(20);
    cout << "r5 setBottom(20): " << r5 << endl;
    
    cout << "r5 left: " << r5.left() << endl;
    cout << "r5 top: " << r5.top() << endl;
    cout << "r5 right: " << r5.right() << endl;
    cout << "r5 bottom: " << r5.bottom() << endl;
    cout << "r5 width: " << r5.width() << endl;
    cout << "r5 height: " << r5.height() << endl;
    cout << "r5 size: " << r5.size() << endl;
                
    cout << "r0 isNull: " << r0.isNull() << endl;
    cout << "r1 isNull: " << r1.isNull() << endl;
    cout << "r6 isNull: " << r6.isNull() << endl;
    cout << "r0 isValid: " << r0.isValid() << endl;
    cout << "r1 isValid: " << r1.isValid() << endl;
    cout << "r6 isValid: " << r6.isValid() << endl;   
    cout << "r0 isEmpty: " << r0.isEmpty() << endl;
    cout << "r1 isEmpty: " << r1.isEmpty() << endl;
    cout << "r6 isEmpty: " << r6.isEmpty() << endl; 
    
    cout << "r6 normalize: " << r6.normalize() << endl;
    
    r5.setX(5);
    cout << "r5 setX(10): " << r5 << endl;
    r5.setY(5);
    cout << "r5 setY(10): " << r5 << endl;
    
    cout << "r5 topLeft: " << r5.topLeft() << endl;
    cout << "r5 bottomRight: " << r5.bottomRight() << endl;
    cout << "r5 topRight: " << r5.topRight() << endl;
    cout << "r5 bottomLeft: " << r5.bottomLeft() << endl;
    cout << "r5 center: " << r5.center() << endl;                                                      
    r5.rect(&x,&y,&w,&h);
    cout << "r5 rect: " << "x=" << x << " y=" << y << " w=" << w << " h=" << h << endl;
    r5.coords(&xp1,&yp1,&xp2,&yp2);
    cout << "r5 coords: " << "xp1=" << xp1 << " yp1=" << yp1 << " xp2=" << xp2 << " yp2=" << yp2 << endl;   
    
    r5.setSize(QSize(12, 12));
    cout << "r5 setSize 12,12: " << r5 << endl;
    r5.setRect(4,4,18,18);
    cout << "r5 setRect 4,4,18,18: " << r5 << endl;
    r5.setCoords(5,5,20,20);
    cout << "r5 setCoords 5,5,20,20: " << r5 << endl;   
    
    r5.moveTopLeft(QPoint(4,4));
    cout << "r5 moveTopLeft 4,4: " << r5 << endl;
    r5.moveBottomRight(QPoint(22,22));
    cout << "r5 moveBottomRight 22,22: " << r5 << endl;
    r5.moveTopRight(QPoint(22,4));
    cout << "r5 moveTopRight 22,4: " << r5 << endl;
    r5.moveCenter(QPoint(15,10));
    cout << "r5 moveCenter 15,10: " << r5 << endl;
    r5.moveBy(5,5);
    cout << "r5 moveBy 5,5: " << r5 << endl;
    
    cout << "r5 contains 15,10: " << r5.contains(15,10) << endl;
    cout << "r5 contains 30,20: " << r5.contains(30,20) << endl;
    cout << "r5 contains 15,10: " << r5.contains(QPoint(15,10)) << endl;
    cout << "r5 contains 30,20: " << r5.contains(QPoint(30,20)) << endl;
    cout << "r5 contains 14,9,5,5: " << r5.contains(QRect(14,9,5,5)) << endl;
    cout << "r5 contains 14,9,20,20: " << r5.contains(QRect(14,9,20,20)) << endl;   
    
    cout << "r5 & (14,9,5,5): " << (QRect(14,9,5,5)) << endl;
    cout << "r1 |= r2: " << (r1|=r2) << endl;
    cout << "r5 &= (14,9,5,5): " << (r5 & QRect(14,9,5,5)) << endl;                                    
    cout << "r1 == r4: "<< (r1 == r4) << endl;
    cout << "r3 == r4: "<< (r3 == r4) << endl;
    cout << "r1 != r4: "<< (r1 != r4) << endl;
    cout << "r3 != r4: "<< (r3 != r4) << endl;


    return 0;
}
