#include <iostream>

#include <qcstring.h>

int main() {

    QCString s0 = QCString();
    QCString s1 = QCString(10);
    QCString s2 = QCString("this is a string");
    QCString s3 = QCString("this is another string", 15);
    QCString s4 = "jazz hype loves two mugs for dire quixotic knobs";
    QCString s5 = s1; 
    QCString s6 = "";
    
    cout << "s0: " << s6 << endl;
    cout << "s1: " << s1 << endl;
    cout << "s2: " << s2 << endl;
    cout << "s3: " << s3 << endl;
    cout << "s4: " << s4 << endl;
    cout << "s5: " << s5 << endl;
    cout << "s6: " << s6 << endl;
    
    cout << "s6 isEmpty: " << s6.isEmpty() << endl;
    cout << "s4 isEmpty: " << s4.isEmpty() << endl;
    
    cout << "s4 isNull: " << s4.isNull() << endl;
    cout << "s0 isNull: " << s0.isNull() << endl;
    cout << "s6 isNull: " << s6.isNull() << endl;
    
    QCString s7 = "THIS WAS A SENTENCE IN ALL UPPERCASE";
    cout << "s7 lower: " << s7.lower() << endl;

    QCString s8 = "I'm going to do a find on this sentence";
    cout << "s8: " << s8 << endl;
    cout << "s8 find \"this\": " << s8.find("this", 0, TRUE) << endl;
    cout << "s8 contains \"o\": " << s8.contains('o', TRUE) << endl;

    QCString s9 = "I'm going to get the length of this string";
    cout << "s9: " << s9 << endl;
    cout << "s9 length: " << s9.length() << endl;
    
    QCString s10 = "I'm going to truncate this string to 15 characters";
    cout << "s10: " << s10 << endl;
    s10.truncate(15);
    cout << "s10 truncate 15: " << s10 << endl;
    
    QCString s11 = "I'm going get 15 characters out of this string starting at position 10";
    cout << "s11: " << s11 << endl;
    cout << "s11 mid 10-25: " << s10.mid(10, 15) << endl; // heh? Seems to return 10-15 instead of 10-25
    
    QCString s12 = "Have a good ";
    cout << "s12: " << s12 << endl;
    s12 += "night";
    cout << "s12 +=: " << s12 << endl;
    s12 += '!';
    cout << "s12 +=: " << s12 << endl;
    
    QCString s13 = "This is a sentence";
    char ch1[20], ch2[20]; 
    strcpy(ch1, "This is a sentence");
    strcpy(ch2, "This is another sentence");
    cout << "s13: " << s13 << endl;
    cout << "ch1: " << ch1 << endl;
    cout << "ch2: " << ch2 << endl;
#if 0
    cout << "ch1 == s13: " << (ch1 == s13) << endl;
    cout << "s13 == ch1: " << (s13 == ch1) << endl;
    cout << "ch2 == s13: " << (ch2 == s13) << endl;
    cout << "s13 == ch2: " << (s13 == ch2) << endl;
    cout << "ch1 != s13: " << (ch1 != s13) << endl;
    cout << "s13 != ch1: " << (s13 != ch1) << endl;
    cout << "ch2 != s13: " << (ch2 != s13) << endl;
    cout << "s13 != ch2: " << (s13 != ch2) << endl;    
#endif

    return 0;
}
