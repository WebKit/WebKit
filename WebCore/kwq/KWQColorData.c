/* ANSI-C code produced by gperf version 3.0.1 */
/* Command-line: gperf -CDEot -L ANSI-C -k '*' -N findColor KWQColorData.gperf  */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "KWQColorData.gperf"
struct Color { const char *name; int RGBValue; };
/* maximum key range = 1053, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (register const char *str, register unsigned int len)
{
  static const unsigned short asso_values[] =
    {
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056,    5,    0,  105,
         0,    0,   10,   40,   40,   10,    0,    0,   15,   60,
         0,    5,  255,   65,    0,   10,   15,  130,  152,  215,
         5,    0,    0, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056, 1056,
      1056, 1056, 1056, 1056, 1056, 1056, 1056
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[19]];
      /*FALLTHROUGH*/
      case 19:
        hval += asso_values[(unsigned char)str[18]];
      /*FALLTHROUGH*/
      case 18:
        hval += asso_values[(unsigned char)str[17]];
      /*FALLTHROUGH*/
      case 17:
        hval += asso_values[(unsigned char)str[16]];
      /*FALLTHROUGH*/
      case 16:
        hval += asso_values[(unsigned char)str[15]];
      /*FALLTHROUGH*/
      case 15:
        hval += asso_values[(unsigned char)str[14]];
      /*FALLTHROUGH*/
      case 14:
        hval += asso_values[(unsigned char)str[13]];
      /*FALLTHROUGH*/
      case 13:
        hval += asso_values[(unsigned char)str[12]];
      /*FALLTHROUGH*/
      case 12:
        hval += asso_values[(unsigned char)str[11]];
      /*FALLTHROUGH*/
      case 11:
        hval += asso_values[(unsigned char)str[10]+1];
      /*FALLTHROUGH*/
      case 10:
        hval += asso_values[(unsigned char)str[9]];
      /*FALLTHROUGH*/
      case 9:
        hval += asso_values[(unsigned char)str[8]];
      /*FALLTHROUGH*/
      case 8:
        hval += asso_values[(unsigned char)str[7]];
      /*FALLTHROUGH*/
      case 7:
        hval += asso_values[(unsigned char)str[6]];
      /*FALLTHROUGH*/
      case 6:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
        hval += asso_values[(unsigned char)str[3]];
      /*FALLTHROUGH*/
      case 3:
        hval += asso_values[(unsigned char)str[2]];
      /*FALLTHROUGH*/
      case 2:
        hval += asso_values[(unsigned char)str[1]];
      /*FALLTHROUGH*/
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval;
}

#ifdef __GNUC__
__inline
#endif
const struct Color *
findColor (register const char *str, register unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 149,
      MIN_WORD_LENGTH = 3,
      MAX_WORD_LENGTH = 20,
      MIN_HASH_VALUE = 3,
      MAX_HASH_VALUE = 1055
    };

  static const struct Color wordlist[] =
    {
#line 123 "KWQColorData.gperf"
      {"red", 0xff0000},
#line 35 "KWQColorData.gperf"
      {"darkred", 0x8b0000},
#line 140 "KWQColorData.gperf"
      {"tan", 0xd2b48c},
#line 88 "KWQColorData.gperf"
      {"linen", 0xfaf0e6},
#line 131 "KWQColorData.gperf"
      {"sienna", 0xa0522d},
#line 62 "KWQColorData.gperf"
      {"indianred", 0xcd5c5c},
#line 141 "KWQColorData.gperf"
      {"teal", 0x008080},
#line 57 "KWQColorData.gperf"
      {"grey", 0x808080},
#line 58 "KWQColorData.gperf"
      {"green", 0x008000},
#line 56 "KWQColorData.gperf"
      {"gray", 0x808080},
#line 28 "KWQColorData.gperf"
      {"darkgrey", 0xa9a9a9},
#line 29 "KWQColorData.gperf"
      {"darkgreen", 0x006400},
#line 8 "KWQColorData.gperf"
      {"beige", 0xf5f5dc},
#line 109 "KWQColorData.gperf"
      {"orange", 0xffa500},
#line 27 "KWQColorData.gperf"
      {"darkgray", 0xa9a9a9},
#line 110 "KWQColorData.gperf"
      {"orangered", 0xff4500},
#line 65 "KWQColorData.gperf"
      {"khaki", 0xf0e68c},
#line 129 "KWQColorData.gperf"
      {"seagreen", 0x2e8b57},
#line 54 "KWQColorData.gperf"
      {"gold", 0xffd700},
#line 33 "KWQColorData.gperf"
      {"darkorange", 0xff8c00},
#line 30 "KWQColorData.gperf"
      {"darkkhaki", 0xbdb76b},
#line 63 "KWQColorData.gperf"
      {"indigo", 0x4b0082},
#line 55 "KWQColorData.gperf"
      {"goldenrod", 0xdaa520},
#line 90 "KWQColorData.gperf"
      {"maroon", 0x800000},
#line 37 "KWQColorData.gperf"
      {"darkseagreen", 0x8fbc8f},
#line 52 "KWQColorData.gperf"
      {"gainsboro", 0xdcdcdc},
#line 86 "KWQColorData.gperf"
      {"lime", 0x00ff00},
#line 59 "KWQColorData.gperf"
      {"greenyellow", 0xadff2f},
#line 26 "KWQColorData.gperf"
      {"darkgoldenrod", 0xb8860b},
#line 136 "KWQColorData.gperf"
      {"slategrey", 0x708090},
#line 50 "KWQColorData.gperf"
      {"forestgreen", 0x228b22},
#line 135 "KWQColorData.gperf"
      {"slategray", 0x708090},
#line 127 "KWQColorData.gperf"
      {"salmon", 0xfa8072},
#line 130 "KWQColorData.gperf"
      {"seashell", 0xfff5ee},
#line 36 "KWQColorData.gperf"
      {"darksalmon", 0xe9967a},
#line 143 "KWQColorData.gperf"
      {"tomato", 0xff6347},
#line 142 "KWQColorData.gperf"
      {"thistle", 0xd8bfd8},
#line 40 "KWQColorData.gperf"
      {"darkslategrey", 0x2f4f4f},
#line 23 "KWQColorData.gperf"
      {"cyan", 0x00ffff},
#line 46 "KWQColorData.gperf"
      {"dimgrey", 0x696969},
#line 39 "KWQColorData.gperf"
      {"darkslategray", 0x2f4f4f},
#line 102 "KWQColorData.gperf"
      {"mistyrose", 0xffe4e1},
#line 45 "KWQColorData.gperf"
      {"dimgray", 0x696969},
#line 25 "KWQColorData.gperf"
      {"darkcyan", 0x008b8b},
#line 10 "KWQColorData.gperf"
      {"black", 0x000000},
#line 89 "KWQColorData.gperf"
      {"magenta", 0xff00ff},
#line 87 "KWQColorData.gperf"
      {"limegreen", 0x32cd32},
#line 19 "KWQColorData.gperf"
      {"coral", 0xff7f50},
#line 31 "KWQColorData.gperf"
      {"darkmagenta", 0x8b008b},
#line 7 "KWQColorData.gperf"
      {"azure", 0xf0ffff},
#line 48 "KWQColorData.gperf"
      {"firebrick", 0xb22222},
#line 12 "KWQColorData.gperf"
      {"blue", 0x0000ff},
#line 106 "KWQColorData.gperf"
      {"oldlace", 0xfdf5e6},
#line 21 "KWQColorData.gperf"
      {"cornsilk", 0xfff8dc},
#line 24 "KWQColorData.gperf"
      {"darkblue", 0x00008b},
#line 105 "KWQColorData.gperf"
      {"navy", 0x000080},
#line 133 "KWQColorData.gperf"
      {"skyblue", 0x87ceeb},
#line 111 "KWQColorData.gperf"
      {"orchid", 0xda70d6},
#line 75 "KWQColorData.gperf"
      {"lightgrey", 0xd3d3d3},
#line 76 "KWQColorData.gperf"
      {"lightgreen", 0x90ee90},
#line 85 "KWQColorData.gperf"
      {"lightyellow", 0xffffe0},
#line 64 "KWQColorData.gperf"
      {"ivory", 0xfffff0},
#line 74 "KWQColorData.gperf"
      {"lightgray", 0xd3d3d3},
#line 34 "KWQColorData.gperf"
      {"darkorchid", 0x9932cc},
#line 125 "KWQColorData.gperf"
      {"royalblue", 0x4169e1},
#line 66 "KWQColorData.gperf"
      {"lavender", 0xe6e6fa},
#line 107 "KWQColorData.gperf"
      {"olive", 0x808000},
#line 132 "KWQColorData.gperf"
      {"silver", 0xc0c0c0},
#line 139 "KWQColorData.gperf"
      {"steelblue", 0x4682b4},
#line 108 "KWQColorData.gperf"
      {"olivedrab", 0x6b8e23},
#line 22 "KWQColorData.gperf"
      {"crimson", 0xdc143c},
#line 79 "KWQColorData.gperf"
      {"lightseagreen", 0x20b2aa},
#line 134 "KWQColorData.gperf"
      {"slateblue", 0x6a5acd},
#line 47 "KWQColorData.gperf"
      {"dodgerblue", 0x1e90ff},
#line 145 "KWQColorData.gperf"
      {"violet", 0xee82ee},
#line 11 "KWQColorData.gperf"
      {"blanchedalmond", 0xffebcd},
#line 146 "KWQColorData.gperf"
      {"violetred", 0xd02090},
#line 5 "KWQColorData.gperf"
      {"aqua", 0x00ffff},
#line 42 "KWQColorData.gperf"
      {"darkviolet", 0x9400d3},
#line 83 "KWQColorData.gperf"
      {"lightslategrey", 0x778899},
#line 9 "KWQColorData.gperf"
      {"bisque", 0xffe4c4},
#line 82 "KWQColorData.gperf"
      {"lightslategray", 0x778899},
#line 14 "KWQColorData.gperf"
      {"brown", 0xa52a2a},
#line 78 "KWQColorData.gperf"
      {"lightsalmon", 0xffa07a},
#line 137 "KWQColorData.gperf"
      {"snow", 0xfffafa},
#line 72 "KWQColorData.gperf"
      {"lightcyan", 0xe0ffff},
#line 124 "KWQColorData.gperf"
      {"rosybrown", 0xbc8f8f},
#line 128 "KWQColorData.gperf"
      {"sandybrown", 0xf4a460},
#line 32 "KWQColorData.gperf"
      {"darkolivegreen", 0x556b2f},
#line 38 "KWQColorData.gperf"
      {"darkslateblue", 0x483d8b},
#line 150 "KWQColorData.gperf"
      {"yellow", 0xffff00},
#line 71 "KWQColorData.gperf"
      {"lightcoral", 0xf08080},
#line 101 "KWQColorData.gperf"
      {"mintcream", 0xf5fffa},
#line 126 "KWQColorData.gperf"
      {"saddlebrown", 0x8b4513},
#line 60 "KWQColorData.gperf"
      {"honeydew", 0xf0fff0},
#line 119 "KWQColorData.gperf"
      {"pink", 0xffc0cb},
#line 70 "KWQColorData.gperf"
      {"lightblue", 0xadd8e6},
#line 16 "KWQColorData.gperf"
      {"cadetblue", 0x5f9ea0},
#line 147 "KWQColorData.gperf"
      {"wheat", 0xf5deb3},
#line 68 "KWQColorData.gperf"
      {"lawngreen", 0x7cfc00},
#line 148 "KWQColorData.gperf"
      {"white", 0xffffff},
#line 3 "KWQColorData.gperf"
      {"aliceblue", 0xf0f8ff},
#line 6 "KWQColorData.gperf"
      {"aquamarine", 0x7fffd4},
#line 18 "KWQColorData.gperf"
      {"chocolate", 0xd2691e},
#line 151 "KWQColorData.gperf"
      {"yellowgreen", 0x9acd32},
#line 103 "KWQColorData.gperf"
      {"moccasin", 0xffe4b5},
#line 80 "KWQColorData.gperf"
      {"lightskyblue", 0x87cefa},
#line 17 "KWQColorData.gperf"
      {"chartreuse", 0x7fff00},
#line 51 "KWQColorData.gperf"
      {"fuchsia", 0xff00ff},
#line 113 "KWQColorData.gperf"
      {"palegreen", 0x98fb98},
#line 61 "KWQColorData.gperf"
      {"hotpink", 0xff69b4},
#line 95 "KWQColorData.gperf"
      {"mediumseagreen", 0x3cb371},
#line 49 "KWQColorData.gperf"
      {"floralwhite", 0xfffaf0},
#line 13 "KWQColorData.gperf"
      {"blueviolet", 0x8a2be2},
#line 100 "KWQColorData.gperf"
      {"midnightblue", 0x191970},
#line 112 "KWQColorData.gperf"
      {"palegoldenrod", 0xeee8aa},
#line 149 "KWQColorData.gperf"
      {"whitesmoke", 0xf5f5f5},
#line 138 "KWQColorData.gperf"
      {"springgreen", 0x00ff7f},
#line 41 "KWQColorData.gperf"
      {"darkturquoise", 0x00ced1},
#line 144 "KWQColorData.gperf"
      {"turquoise", 0x40e0d0},
#line 15 "KWQColorData.gperf"
      {"burlywood", 0xdeb887},
#line 118 "KWQColorData.gperf"
      {"peru", 0xcd853f},
#line 77 "KWQColorData.gperf"
      {"lightpink", 0xffb6c1},
#line 53 "KWQColorData.gperf"
      {"ghostwhite", 0xf8f8ff},
#line 67 "KWQColorData.gperf"
      {"lavenderblush", 0xfff0f5},
#line 92 "KWQColorData.gperf"
      {"mediumblue", 0x0000cd},
#line 93 "KWQColorData.gperf"
      {"mediumorchid", 0xba55d3},
#line 84 "KWQColorData.gperf"
      {"lightsteelblue", 0xb0c4de},
#line 81 "KWQColorData.gperf"
      {"lightslateblue", 0x8470ff},
#line 44 "KWQColorData.gperf"
      {"deepskyblue", 0x00bfff},
#line 73 "KWQColorData.gperf"
      {"lightgoldenrodyellow", 0xfafad2},
#line 120 "KWQColorData.gperf"
      {"plum", 0xdda0dd},
#line 104 "KWQColorData.gperf"
      {"navajowhite", 0xffdead},
#line 96 "KWQColorData.gperf"
      {"mediumslateblue", 0x7b68ee},
#line 99 "KWQColorData.gperf"
      {"mediumvioletred", 0xc71585},
#line 115 "KWQColorData.gperf"
      {"palevioletred", 0xd87093},
#line 91 "KWQColorData.gperf"
      {"mediumaquamarine", 0x66cdaa},
#line 69 "KWQColorData.gperf"
      {"lemonchiffon", 0xfffacd},
#line 43 "KWQColorData.gperf"
      {"deeppink", 0xff1493},
#line 20 "KWQColorData.gperf"
      {"cornflowerblue", 0x6495ed},
#line 121 "KWQColorData.gperf"
      {"powderblue", 0xb0e0e6},
#line 4 "KWQColorData.gperf"
      {"antiquewhite", 0xfaebd7},
#line 97 "KWQColorData.gperf"
      {"mediumspringgreen", 0x00fa9a},
#line 114 "KWQColorData.gperf"
      {"paleturquoise", 0xafeeee},
#line 122 "KWQColorData.gperf"
      {"purple", 0x800080},
#line 98 "KWQColorData.gperf"
      {"mediumturquoise", 0x48d1cc},
#line 117 "KWQColorData.gperf"
      {"peachpuff", 0xffdab9},
#line 94 "KWQColorData.gperf"
      {"mediumpurple", 0x9370d8},
#line 116 "KWQColorData.gperf"
      {"papayawhip", 0xffefd5}
    };

  static const short lookup[] =
    {
       -1,  -1,  -1,   0,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,   1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,   2,  -1,  -1,  -1,  -1,  -1,  -1,
        3,   4,  -1,  -1,   5,  -1,  -1,  -1,  -1,   6,
       -1,  -1,  -1,  -1,   7,   8,  -1,  -1,  -1,   9,
       -1,  -1,  -1,  10,  11,  12,  13,  -1,  14,  15,
       16,  -1,  -1,  17,  18,  19,  -1,  -1,  -1,  20,
       -1,  21,  -1,  -1,  22,  -1,  -1,  -1,  -1,  -1,
       -1,  23,  24,  -1,  25,  -1,  -1,  -1,  -1,  26,
       -1,  27,  -1,  28,  29,  -1,  30,  -1,  -1,  31,
       -1,  32,  -1,  33,  -1,  -1,  -1,  -1,  -1,  -1,
       34,  35,  36,  37,  38,  -1,  -1,  39,  40,  41,
       -1,  -1,  42,  43,  -1,  -1,  -1,  -1,  -1,  -1,
       44,  -1,  45,  -1,  46,  47,  48,  -1,  -1,  -1,
       49,  -1,  -1,  -1,  50,  -1,  -1,  -1,  -1,  51,
       -1,  -1,  52,  53,  -1,  -1,  -1,  -1,  54,  -1,
       -1,  55,  56,  -1,  -1,  -1,  57,  -1,  -1,  58,
       59,  60,  61,  -1,  62,  63,  -1,  -1,  -1,  64,
       65,  -1,  -1,  -1,  -1,  -1,  -1,  66,  -1,  -1,
       -1,  -1,  -1,  67,  68,  -1,  69,  70,  71,  72,
       73,  -1,  -1,  74,  75,  -1,  76,  -1,  -1,  77,
       -1,  -1,  78,  -1,  -1,  -1,  -1,  -1,  -1,  79,
       -1,  80,  -1,  -1,  81,  82,  -1,  -1,  -1,  -1,
       -1,  83,  -1,  -1,  84,  -1,  -1,  -1,  -1,  85,
       -1,  -1,  -1,  -1,  86,  87,  -1,  -1,  -1,  -1,
       -1,  88,  -1,  89,  -1,  -1,  90,  -1,  -1,  -1,
       91,  -1,  -1,  -1,  92,  -1,  93,  -1,  94,  95,
       -1,  -1,  -1,  -1,  96,  -1,  -1,  -1,  -1,  97,
       98,  -1,  -1,  -1,  99, 100,  -1,  -1,  -1, 101,
      102,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1, 103,  -1, 104,  -1, 105, 106,
       -1,  -1,  -1,  -1,  -1, 107,  -1, 108,  -1,  -1,
       -1,  -1,  -1,  -1, 109,  -1,  -1,  -1,  -1,  -1,
       -1,  -1, 110,  -1,  -1,  -1,  -1,  -1,  -1, 111,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1, 112, 113,  -1, 114,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1, 115,  -1, 116,  -1,  -1,  -1,  -1,
       -1, 117,  -1, 118, 119,  -1,  -1,  -1,  -1, 120,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 121,
       -1,  -1,  -1,  -1, 122,  -1,  -1,  -1,  -1,  -1,
      123,  -1, 124,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1, 125,  -1,  -1,  -1,  -1,
       -1,  -1, 126,  -1, 127,  -1,  -1,  -1,  -1, 128,
       -1, 129,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      130,  -1,  -1,  -1, 131,  -1,  -1,  -1, 132,  -1,
       -1,  -1,  -1,  -1,  -1, 133,  -1,  -1,  -1,  -1,
       -1,  -1, 134,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1, 135, 136,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1, 137,  -1,  -1,  -1,  -1,  -1, 138,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 139,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
      140,  -1, 141,  -1,  -1,  -1,  -1, 142,  -1,  -1,
       -1,  -1,  -1, 143,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1, 144, 145,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 146,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1, 147,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1, 148
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register int index = lookup[key];

          if (index >= 0)
            {
              register const char *s = wordlist[index].name;

              if (*str == *s && !strcmp (str + 1, s + 1))
                return &wordlist[index];
            }
        }
    }
  return 0;
}
