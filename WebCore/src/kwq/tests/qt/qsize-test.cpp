#include <iostream>

#include <qsize.h>

int main() {

    QSize s0;
    QSize s1 = QSize(1,1);
    QSize s2 = QSize(3,3);
    QSize s3 = s1 + s2;
    QSize s4 = s3;
    QSize s5 = s3;

    s5.setWidth(10);
    s5.setHeight(10);
    s5 += s1;
    s5 -= s2;
    s5 *= 3;
    s5 /= 2;
    
    cout << s1 << endl;
    cout << s2 << endl;
    cout << s3 << endl;
    cout << s0.isNull() << endl;
    cout << s1.isNull() << endl;
    cout << (s1 == s4) << endl;
    cout << (s3 == s4) << endl;
    cout << (s1 != s4) << endl;
    cout << (s3 != s4) << endl;
    cout << s5 << endl;

    return 0;
}
