#include <iostream>

#include <qstring.h>
#include <qregexp.h>

int main() {

    QString s1 = "abcdefghijklmnopqrstuvwxyz";
    QString s2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    QString s3 = "AAABBBCCC";
    
    QRegExp r1 = QRegExp("^ab");    
    QRegExp r2 = QRegExp("def");    
    QRegExp r3 = QRegExp("[A-Z]");    
    QRegExp r4 = QRegExp("B+C");    

    cout << s1 << " =~ " << r1.pattern() << ": " 
        << r1.match(s1) << endl;

    cout << s2 << " =~ " << r1.pattern() << ": " 
        << r1.match(s2) << endl;

    cout << s1 << " =~ " << r2.pattern() << ": " 
        << r2.match(s1) << endl;

    cout << s2 << " =~ " << r2.pattern() << ": " 
        << r2.match(s2) << endl;

    cout << s1 << " =~ " << r3.pattern() << ": " 
        << r3.match(s1) << endl;
    
    cout << s2 << " =~ " << r3.pattern() << ": " 
        << r3.match(s2) << endl;
    
    cout << s3 << " =~ " << r3.pattern() << ": " 
        << r3.match(s3) << endl;

    cout << s3 << " =~ " << r4.pattern() << ": " 
        << r4.match(s3) << endl;

    return 0;
}
