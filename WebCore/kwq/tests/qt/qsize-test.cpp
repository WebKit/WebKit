#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <qsize.h>

int main() {

    QSize s0;
    QSize s1 = QSize(1,1);
    QSize s2 = QSize(3,3);
    QSize s3 = s1 + s2;
    QSize s4 = s3;
    QSize s5 = s3;
    QSize s6 = QSize(0,0);
    QSize s7 = QSize(20,20);
    
    cout << "s1: " << s1 << endl;
    cout << "s2: " << s2 << endl;
    cout << "s3 = s1+s2: " << s3 << endl;
    cout << "s4 = s3: " << s4 << endl;
    cout << "s5 = s3: " << s5 << endl;
    cout << "s6: " << s6 << endl;
    cout << "s7: " << s7 << endl;
    
    s5.setWidth(10);
    cout << "s5 setWidth(10): " << s5 << endl;
    s5.setHeight(10);
    cout << "s5 setHeight(10): " << s5 << endl;
    s5 += s1;
    cout << "s5 += s1: " << s5 << endl;
    s5 -= s2;
    cout << "s5 -= s2: " << s5 << endl;
    s5 *= 3;
    cout << "s5 *= 3: " << s5 << endl; 
    s5 /= 2;
    cout << "s5 /= 2: " << s5 << endl;
    s7 *= 3.33;
    cout << "s7 *= 3.33: " << s7 << endl; 
    s7 /= 3.33;
    cout << "s7 /= 3.33: " << s7 << endl;
        
    cout << "s5 width: " << s5.width() << endl;
    cout << "s5 height: " << s5.height() << endl;
    
    s5 = s5.expandedTo(QSize(10,15));
    cout << "s5 expandedTo 10,15: " << s5 << endl;
    
    s5 = s5.boundedTo(QSize(10,15));
    cout << "s5 boundedTo 10,15: " << s5 << endl;
    
    s5.transpose();
    cout << "s5 transpose: " << s5 << endl;
    
    cout << "s0 isNull: " << s0.isNull() << endl;
    cout << "s1 isNull: " << s1.isNull() << endl;
    cout << "s6 isNull: " << s6.isNull() << endl;
    cout << "s0 isValid: " << s0.isValid() << endl;
    cout << "s1 isValid: " << s1.isValid() << endl;
    cout << "s6 isValid: " << s6.isValid() << endl;
    cout << "s0 isEmpty: " << s0.isEmpty() << endl;
    cout << "s1 isEmpty: " << s1.isEmpty() << endl;
    cout << "s6 isEmpty: " << s6.isEmpty() << endl;
    cout << "s1 == s4: " << (s1 == s4) << endl;
    cout << "s3 == s4: " << (s3 == s4) << endl;
    cout << "s1 != s4: " << (s1 != s4) << endl;
    cout << "s3 != s4: " << (s3 != s4) << endl;

    cout << "s3 * 3.33: " << (s3 * 3.33) << endl;
    cout << "3.33 * s3: " << (3.33 * s3) << endl;
    cout << "2 * s4: " << (2 * s4) << endl;
    cout << "s4 * 2: " << (s4 * 2) << endl;
    cout << "s4 / 2: " << (s4 / 2) << endl;
    cout << "s4 / 2.22: " << (s4 / 2.22) << endl;

    return 0;
}
