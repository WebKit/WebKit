#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <qstring.h>
#include <qregexp.h>

int main() {
    
    QChar c[4]; 
    QByteArray ba = QByteArray(5);
    ba.fill('e', -1);
    QCString qcs = QCString("hello there");
    //char ch='a';
    
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
    //QString s10 = ch;
    
    cout << s1 << endl;
    cout << s2 << endl;
    cout << s3 << endl;
    cout << s4 << endl;
    cout << s5 << endl;
    cout << s6 << endl;
    cout << s7 << endl;
    cout << s8 << endl;
    cout << s9 << endl;
    //cout << s10 << endl;
    
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
    //cout << s7.findRev(c[0], 10, TRUE) << endl;
    //cout << s7.findRev(qre, 10) << endl;
    //cout << s7.findRev(s12, -1, TRUE) << endl;
    //cout << s7.findRev('t', 10) << endl;
    //cout << s7.findRev('t', 10, TRUE) << endl;
    
    //cout << s7.contains(c[0], TRUE) << endl;
    //cout << s7.contains(qre) << endl;
    cout << s13.contains(s14, TRUE) << endl;
    //cout << s13.contains('b', TRUE) << endl;
    cout << s13.contains(blah, TRUE) << endl;
    
    cout << s13.left(4) << endl;
    cout << s13.right(4) << endl;
    cout << s13.mid(5,4) << endl;
    //cout << s14.leftJustify(8,'.') << endl;
    //cout << s14.rightJustify(8,'.') << endl;
    
    QString s15 = "Hello %1. How has your %2 been?";
    double my_double =22.45;
    int my_int = 356;
    long my_long = 23390082;
    short my_short = 3456;
    uint my_unit = 234234224;
    ulong my_ulong = 23409283;
    ushort my_ushort = 43242;
    float my_float = 223.2342;
    
    cout << s15.arg(s14).arg(s14) << endl;
    cout << s15.arg(s14, 5).arg(s14, 5) << endl; // fieldwidth only add one extra space. hmmmm.
    //cout << s15.arg(c[2]).arg(c[2]) << endl;
    cout << s15.arg("Gramps").arg("morning") << endl;
    cout << s15.arg(my_double).arg(my_double) << endl;
    cout << s15.arg(my_int).arg(my_long) << endl;
    cout << s15.arg(my_short).arg(my_unit) << endl;
    cout << s15.arg(my_ulong).arg(my_ushort) << endl;
    
    QString s16 = "THIS WAS A STRING IN UPPERCASE";
    QString s17 = "this was a string in lowercase";
    cout << s16.lower() << endl;
    //cout << s17.upper() << endl;
    
    QString s18 =" This    was a  string   with a lot of       spaces ";
    cout << s18.stripWhiteSpace() << endl; //why is white space still there?
    cout << s18.simplifyWhiteSpace() << endl; //this works
    
    QString s19 = "221545";
    //cout << s19.toDouble() << endl;   //Am I properly testing this?
    cout << s19.toFloat() << endl;
    cout << s19.toInt() << endl;
    cout << s19.toLong() << endl;
    //cout << s19.toShort() << endl;
    cout << s19.toUInt() << endl;
    //cout << s19.toULong() << endl;
    //cout << s19.toUShort() << endl;
    
    QString s20;
    cout << s20.sprintf("This is a %s. %d, %f", "test", 44, 56.78) << endl;

    QString s21 = "Apple Computer Inc.";
    QString s22 = "Apple";
    QString s23 = "MS";
    cout << s21.startsWith(s22) << endl;
    cout << s21.startsWith(s23) << endl;
    
    QString s24 = "The answer is: ";
    QChar c2 = '2';
    cout << s24.insert(15, c2) << endl;
    //cout << s24.insert(15, c, 4) << endl;
    //cout << s24.insert(15, s22) << endl;
    //cout << s24.insert(15, 'c') << endl;
    cout << s24.append(c2) << endl;
    //cout << s24.append('c') << endl;
    cout << s24.append(s22) << endl;    
    cout << s24.prepend(c2) << endl;
    //cout << s24.prepend('c') << endl;
    cout << s24.prepend(s22) << endl;
    cout << s24.remove(0, 7) << endl;
    
    QString s25 = "There are a lot of a's in a sentence such as this one";
    QString s26 = "bell";
    cout << s25.replace( QRegExp("a"), "e" ) << endl; 
    //cout << s25.replace(1, 4, c, 4) << endl;
    //cout << s25.replace(12, 4, s26) << endl;
    
    QString s27;
    cout << s27.setNum(my_double) << endl;
    cout << s27.setNum(my_int) << endl;
    cout << s27.setNum(my_int) << endl;
    cout << s27.setNum(my_long) << endl;
    cout << s27.setNum(my_short) << endl;
    cout << s27.setNum(my_unit) << endl;
    cout << s27.setNum(my_ulong) << endl;
    cout << s27.setNum(my_ushort) << endl;
    cout << s27.setNum(my_float) << endl;
    
    QString s28;
    QString s29 = "a string that is a little long";
    cout << !s28 << endl;
    cout << !s29 << endl;
    
    QString s30 = "abcde";
    QChar c3='f';
    QChar ch2='g';
    QString s31 = "hijk";
    cout << (s30 += c3) << endl;
    cout << (s30 += ch2) << endl;
    cout << (s30 += s31) << endl;
    
    
    // test operators
    
    QString s33 = "This is a string";
    QString s34 = "This is a longer string";
    char c4[25];
    strcpy(c4, "This is an even longer string");
    cout << (s33 < c4) << endl;
    cout << (c4 < s33) << endl;
    cout << (s33 < s34) << endl;
    cout << (s33 > c4) << endl;
    //cout << (c4 > s33) << endl; //compiler says this is not a supported function. 
                                  //Documentation says bool operator> (const char * s1, const QString & s2) 
    cout << (s33 > s34) << endl;
    cout << (s33 == c4) << endl;
    cout << (c4 == s33) << endl;
    cout << (s33 == s34) << endl;
    cout << (s33 != c4) << endl;
    cout << (c4 != s33) << endl;
    cout << (s33 != s34) << endl;    
    cout << (s33 <= c4) << endl;
    cout << (c4 <= s33) << endl;
    cout << (s33 <= s34) << endl;
    cout << (s33 >= c4) << endl;
    cout << (c4 >= s33) << endl;
    cout << (s33 >= s34) << endl;
    
    
        
    return 0;
}
