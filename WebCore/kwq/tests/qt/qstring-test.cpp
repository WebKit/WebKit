#include <iostream>
#include <qstring.h>
#include <qregexp.h>

int main() {
    
    QChar c[4]; 
    QByteArray ba = QByteArray(5);
    ba.fill('e', -1);
    QCString qcs = QCString("hello there");
    char ch='a';
    
    c[0] = QChar('t');
    c[1] = QChar('e');
    c[2] = QChar('s');
    c[3] = QChar('t');
    
    QString s0 = QString();
    QString s1 = QString(c[0]);
    QString s2 = QString(s1);
    QString s3 = QString(ba);
    QString s4 = QString(c, 4);
    QString s5 = QString("this is a string");
    QString s6 = s4;
    QString s7 = "yet another string";
    QString s8 = qcs;
    QString s9 = c[1];
    QString s10 = &ch;
    
    cout << s1 << endl;
    cout << s2 << endl;
    cout << s3 << endl;
    cout << s4 << endl;
    cout << s5 << endl;
    cout << s6 << endl;
    cout << s7 << endl;
    cout << s8 << endl;
    cout << s9 << endl;
    cout << s10 << endl;
    
    QString s11 = QString(""); // make an empty string
    
    cout << s0.isNull() << endl;
    cout << s5.isNull() << endl;
    cout << s11.isEmpty() << endl;
    cout << s5.isEmpty() << endl;
    cout << s7.length() << endl;
    
    s5.truncate(8);
    cout << s5 << endl;
    
    s0.fill(c[3], 5);
    cout << s0 << endl;
    
    QRegExp qre = QRegExp(s1, TRUE, FALSE);
    QString s12 = QString("string");
    QString s13 = QString("blah blah blah");
    QString s14 = QString("blah");
    char blah[4];
    strcpy(blah, "blah");
    
    cout << s7.find(c[2], 0, FALSE) << endl; // find functions
    cout << s7.find(qre, 0) << endl;
    cout << s7.find(s12, 0, TRUE) << endl;
    cout << s7.find("string", 0) << endl;
    cout << s7.find("string", 0, TRUE) << endl;
    cout << s7.findRev(c[0], 10, TRUE) << endl;
    cout << s7.findRev(qre, 10) << endl;
    cout << s7.findRev(s12, -1, TRUE) << endl;
    cout << s7.findRev('t', 10) << endl;
    cout << s7.findRev('t', 10, TRUE) << endl;
    
    cout << s7.contains(c[0], TRUE) << endl;
    cout << s7.contains(qre) << endl;
    cout << s13.contains(s14, TRUE) << endl;
    cout << s13.contains('b', TRUE) << endl;
    cout << s13.contains(blah, TRUE) << endl;
    
    cout << s13.left(4) << endl;
    cout << s13.right(4) << endl;
    cout << s13.mid(5,4) << endl;
    return 0;
}
