#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <qstring.h>

int main() {

    char c_char = 'a';
    uchar c_uchar = 'b';
    ushort c_ushort = 'D';
    short c_short = ' ';
    uint c_uint = '3';
    int c_int = '-';
    
    QChar q0 = QChar();
    QChar q1 = QChar(c_char);
    QChar q2 = QChar(c_uchar);
    //QChar q3 = QChar(c_uchar, 0);
    QChar q4 = QChar(q1);
    QChar q5 = QChar(c_ushort);
    QChar q6 = QChar(c_short);
    QChar q7 = QChar(c_uint);
    QChar q8 = QChar(c_int);
    
    cout << q1 << endl;
    cout << q2 << endl;
    //cout << q3 << endl;
    cout << q4 << endl;
    cout << q5 << endl;
    cout << q6 << endl;
    cout << q7 << endl;
    cout << q8 << endl;
    
    cout << q5.latin1() << endl;
    cout << q1.unicode() << endl;
    cout << q2.cell() << endl;
    cout << q2.row() << endl;
    cout << q2.operator char() << endl;
    //cout << q7.isNumber() << endl;
    //cout << q8.isNumber() << endl;
    cout << q8.isPunct() << endl;
    cout << q7.isPunct() << endl;
    cout << q6.isSpace() << endl;
    cout << q5.isSpace() << endl;
    cout << q0.isNull() << endl;
    cout << q1.isNull() << endl;
    //cout << q7.isNumber() << endl;
    //cout << q8.isNumber() << endl;
    cout << q7.isDigit() << endl;
    cout << q8.isDigit() << endl;
    cout << q7.isLetterOrNumber() << endl;
    cout << q8.isLetterOrNumber() << endl;
    //cout << q3.isPrint() << endl;
    //cout << q3.isMark() << endl;
    cout << q5.lower() << endl;
    cout << q1.upper() << endl;
    
    cout << (q2 <= c_char) << endl;
    cout << (q2 <= q1) << endl;
    cout << (c_char <= q2) << endl;
    cout << (q2 >= q1) << endl;
    cout << (q2 >= c_char) << endl;
    cout << (c_char >= q2) << endl;
    cout << (q2 != c_char) << endl;
    cout << (q2 != q1) << endl;
    cout << (c_char != q2) << endl;
    cout << (q2 < c_char) << endl;
    cout << (q2 < q1) << endl;
    cout << (c_char < q2) << endl;
    cout << (q2 > c_char) << endl;
    cout << (q2 > q1) << endl;
    cout << (c_char > q2) << endl;
    cout << (q2 == c_char) << endl;
    cout << (q2 == q1) << endl;
    cout << (c_char == q2) << endl;
    
    return 0;
}
