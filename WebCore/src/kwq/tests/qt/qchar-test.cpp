#include <iostream>

#include <qstring.h>

int main() {

    char c_char = 'a';
    uchar c_uchar = 'b';
    ushort c_ushort = 'd';
    short c_short = ' ';
    uint c_uint = 'f';
    int c_int = '-';
    
    QChar q0 = QChar();
    QChar q1 = QChar(c_char);
    QChar q2 = QChar(c_uchar);
    QChar q3 = QChar(c_uchar, 0);
    QChar q4 = QChar(q1);
    QChar q5 = QChar(c_ushort);
    QChar q6 = QChar(c_short);
    QChar q7 = QChar(c_uint);
    QChar q8 = QChar(c_int);
    
    cout << q1 << endl;
    cout << q2 << endl;
    cout << q3 << endl;
    cout << q4 << endl;
    cout << q5 << endl;
    cout << q6 << endl;
    cout << q7 << endl;
    cout << q8 << endl;
    cout << q8.isPunct() << endl;
    cout << q7.isPunct() << endl;
    cout << q6.isSpace() << endl;
    cout << q5.isSpace() << endl;
    
    return 0;
}
