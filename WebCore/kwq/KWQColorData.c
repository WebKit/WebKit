/* ANSI-C code produced by gperf version 2.7.2 */
/* Command-line: gperf -CDEot -L ANSI-C -k '*' -N findColor KWQColorData.gperf  */
struct Color { const char *name; int RGBValue; };
/* maximum key range = 1178, duplicates = 1 */

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
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181,   10,  249,   55,
         0,    0,    0,    5,   10,    0,    0,   15,    0,  110,
         0,    0,  250,    5,    0,   10,   15,  150,  148,  240,
      1181,  140,    5, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181, 1181,
      1181, 1181, 1181, 1181, 1181, 1181
    };
  register int hval = len;

  switch (hval)
    {
      default:
      case 20:
        hval += asso_values[(unsigned char)str[19]];
      case 19:
        hval += asso_values[(unsigned char)str[18]];
      case 18:
        hval += asso_values[(unsigned char)str[17]];
      case 17:
        hval += asso_values[(unsigned char)str[16]];
      case 16:
        hval += asso_values[(unsigned char)str[15]];
      case 15:
        hval += asso_values[(unsigned char)str[14]];
      case 14:
        hval += asso_values[(unsigned char)str[13]];
      case 13:
        hval += asso_values[(unsigned char)str[12]];
      case 12:
        hval += asso_values[(unsigned char)str[11]];
      case 11:
        hval += asso_values[(unsigned char)str[10]];
      case 10:
        hval += asso_values[(unsigned char)str[9]];
      case 9:
        hval += asso_values[(unsigned char)str[8]];
      case 8:
        hval += asso_values[(unsigned char)str[7]];
      case 7:
        hval += asso_values[(unsigned char)str[6]];
      case 6:
        hval += asso_values[(unsigned char)str[5]];
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      case 4:
        hval += asso_values[(unsigned char)str[3]];
      case 3:
        hval += asso_values[(unsigned char)str[2]];
      case 2:
        hval += asso_values[(unsigned char)str[1]];
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
      MAX_HASH_VALUE = 1180
    };

  static const struct Color wordlist[] =
    {
      {"red", 0xff0000},
      {"linen", 0xfaf0e6},
      {"gold", 0xffd700},
      {"green", 0x008000},
      {"indigo", 0x4b0082},
      {"goldenrod", 0xdaa520},
      {"indianred", 0xcd5c5c},
      {"orange", 0xffa500},
      {"orangered", 0xff4500},
      {"sienna", 0xa0522d},
      {"tan", 0xd2b48c},
      {"teal", 0x008080},
      {"darkred", 0x8b0000},
      {"seagreen", 0x2e8b57},
      {"darkgreen", 0x006400},
      {"forestgreen", 0x228b22},
      {"darkgoldenrod", 0xb8860b},
      {"lightgreen", 0x90ee90},
      {"seashell", 0xfff5ee},
      {"darkorange", 0xff8c00},
      {"khaki", 0xf0e68c},
      {"thistle", 0xd8bfd8},
      {"darkseagreen", 0x8fbc8f},
      {"lightseagreen", 0x20b2aa},
      {"coral", 0xff7f50},
      {"orchid", 0xda70d6},
      {"oldlace", 0xfdf5e6},
      {"darkkhaki", 0xbdb76b},
      {"cornsilk", 0xfff8dc},
      {"darkorchid", 0x9932cc},
      {"lightcoral", 0xf08080},
      {"lime", 0x00ff00},
      {"limegreen", 0x32cd32},
      {"maroon", 0x800000},
      {"salmon", 0xfa8072},
      {"grey", 0x808080},
      {"olive", 0x808000},
      {"chocolate", 0xd2691e},
      {"tomato", 0xff6347},
      {"magenta", 0xff00ff},
      {"gray", 0x808080},
      {"silver", 0xc0c0c0},
      {"darksalmon", 0xe9967a},
      {"lavender", 0xe6e6fa},
      {"violet", 0xee82ee},
      {"azure", 0xf0ffff},
      {"lightsalmon", 0xffa07a},
      {"violetred", 0xd02090},
      {"darkgrey", 0xa9a9a9},
      {"aqua", 0x00ffff},
      {"crimson", 0xdc143c},
      {"lightgrey", 0xd3d3d3},
      {"darkmagenta", 0x8b008b},
      {"lemonchiffon", 0xfffacd},
      {"darkgray", 0xa9a9a9},
      {"slategrey", 0x708090},
      {"darkolivegreen", 0x556b2f},
      {"lightgray", 0xd3d3d3},
      {"darkviolet", 0x9400d3},
      {"slategray", 0x708090},
      {"cyan", 0x00ffff},
      {"darkslategrey", 0x2f4f4f},
      {"lightslategrey", 0x778899},
      {"darkslategray", 0x2f4f4f},
      {"lightslategray", 0x778899},
      {"darkcyan", 0x008b8b},
      {"fuchsia", 0xff00ff},
      {"lightcyan", 0xe0ffff},
      {"moccasin", 0xffe4b5},
      {"snow", 0xfffafa},
      {"beige", 0xf5f5dc},
      {"chartreuse", 0x7fff00},
      {"dimgrey", 0x696969},
      {"lawngreen", 0x7cfc00},
      {"pink", 0xffc0cb},
      {"white", 0xffffff},
      {"dimgray", 0x696969},
      {"palegreen", 0x98fb98},
      {"palegoldenrod", 0xeee8aa},
      {"wheat", 0xf5deb3},
      {"springgreen", 0x00ff7f},
      {"gainsboro", 0xdcdcdc},
      {"floralwhite", 0xfffaf0},
      {"ivory", 0xfffff0},
      {"mistyrose", 0xffe4e1},
      {"hotpink", 0xff69b4},
      {"navy", 0x000080},
      {"lightpink", 0xffb6c1},
      {"aquamarine", 0x7fffd4},
      {"mintcream", 0xf5fffa},
      {"ghostwhite", 0xf8f8ff},
      {"firebrick", 0xb22222},
      {"black", 0x000000},
      {"turquoise", 0x40e0d0},
      {"darkturquoise", 0x00ced1},
      {"yellow", 0xffff00},
      {"greenyellow", 0xadff2f},
      {"yellowgreen", 0x9acd32},
      {"honeydew", 0xf0fff0},
      {"blue", 0x0000ff},
      {"peru", 0xcd853f},
      {"mediumseagreen", 0x3cb371},
      {"whitesmoke", 0xf5f5f5},
      {"dodgerblue", 0x1e90ff},
      {"olivedrab", 0x6b8e23},
      {"bisque", 0xffe4c4},
      {"lightyellow", 0xffffe0},
      {"darkblue", 0x00008b},
      {"steelblue", 0x4682b4},
      {"lightgoldenrodyellow", 0xfafad2},
      {"palevioletred", 0xd87093},
      {"lightblue", 0xadd8e6},
      {"slateblue", 0x6a5acd},
      {"navajowhite", 0xffdead},
      {"mediumorchid", 0xba55d3},
      {"antiquewhite", 0xfaebd7},
      {"blanchedalmond", 0xffebcd},
      {"lightsteelblue", 0xb0c4de},
      {"darkslateblue", 0x483d8b},
      {"aliceblue", 0xf0f8ff},
      {"lightslateblue", 0x8470ff},
      {"cadetblue", 0x5f9ea0},
      {"brown", 0xa52a2a},
      {"plum", 0xdda0dd},
      {"saddlebrown", 0x8b4513},
      {"deeppink", 0xff1493},
      {"mediumvioletred", 0xc71585},
      {"midnightblue", 0x191970},
      {"royalblue", 0x4169e1},
      {"skyblue", 0x87ceeb},
      {"blueviolet", 0x8a2be2},
      {"lavenderblush", 0xfff0f5},
      {"paleturquoise", 0xafeeee},
      {"lightskyblue", 0x87cefa},
      {"rosybrown", 0xbc8f8f},
      {"purple", 0x800080},
      {"mediumspringgreen", 0x00fa9a},
      {"sandybrown", 0xf4a460},
      {"mediumaquamarine", 0x66cdaa},
      {"cornflowerblue", 0x6495ed},
      {"mediumturquoise", 0x48d1cc},
      {"peachpuff", 0xffdab9},
      {"mediumblue", 0x0000cd},
      {"burlywood", 0xdeb887},
      {"mediumslateblue", 0x7b68ee},
      {"deepskyblue", 0x00bfff},
      {"powderblue", 0xb0e0e6},
      {"mediumpurple", 0x9370d8},
      {"papayawhip", 0xffefd5}
    };

  static const short lookup[] =
    {
        -1,   -1,   -1,    0,   -1,    1,   -1,   -1,
        -1,    2,    3,    4,   -1,   -1,    5,   -1,
        -1,   -1,   -1,    6,   -1,    7,   -1,   -1,
         8,   -1,    9,   -1,   10,   11,   -1,   -1,
        12,   13,   -1,   -1,   -1,   -1,   -1,   14,
        -1,   15,   -1,   16,   -1,   17,   -1,   -1,
        18,   -1,   19,   -1,   -1,   -1,   -1,   20,
        -1,   21,   -1,   -1,   -1,   -1,   22,   -1,
        -1,   -1,   -1,   -1,   23,   -1,   24,   25,
        26,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   27,   -1,   -1,   -1,
        28,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   29,   -1,   -1,   -1,
        -1,   30,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   31,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   32,   -1,   33,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        34,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   35,   -1,   -1,
        -1,   36,   37,   -1,   38,   39,   -1,   40,
        -1,   -1,   -1,   -1,   41,   42,   43,   -1,
        -1,   44,   45,   46,   47,   -1,   -1,   -1,
        -1,   -1,   48,   49,   -1,   -1,   50,   -1,
        51,   -1,   52,   53,   54,   55,   -1,   -1,
        56,   -1,   57,   -1,   -1,   -1,   58,   59,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   60,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   61,   -1,   -1,   -1,   -1,   -1,
        62,   -1,   -1,   -1,   63,   -1,   -1,   -1,
        -1,   -1,   64,   -1,   -1,   -1,   65,   -1,
        -1,   -1,   66,   -1,   67,   -1,   -1,   -1,
        68,   -1,   -1,   -1,   -1,   -1,   69,   -1,
        -1,   -1,   -1,   70,   71,   -1,   72,   -1,
        73,   -1,   -1,   -1,   -1,   74,   75,   -1,
        76,   -1,   77,   -1,   -1,   -1,   78,   -1,
        79,   80,   -1,   81,   -1,   -1,   82,   -1,
        -1,   -1,   -1,   -1,   -1,   83,   84,   -1,
        -1,   85,   -1,   -1,   -1,   -1,   86,   -1,
        87,   88,   -1,   -1,   -1,   89,   -1,   -1,
        -1,   -1,   -1,   90,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        91,   -1,   -1,   -1,   -1,   -1,   92,   -1,
        -1,   -1,   -1,   93,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        94,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   95,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1, -549,   -1,   98,  -53,
        -2,   -1,   -1,   99,  100,   -1,   -1,   -1,
        -1,  101,  102,   -1,   -1,   -1,  103,   -1,
       104,   -1,   -1,   -1,  105,  106,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       107,  108,   -1,  109,  110,   -1,  111,   -1,
        -1,   -1,   -1,  112,  113,   -1,   -1,  114,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  115,  116,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  117,   -1,   -1,   -1,
       118,  119,   -1,   -1,   -1,   -1,  120,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       121,   -1,   -1,   -1,   -1,   -1,  122,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  123,   -1,   -1,   -1,   -1,   -1,
       124,   -1,   -1,  125,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  126,   -1,   -1,  127,
        -1,   -1,   -1,   -1,   -1,   -1,  128,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  129,  130,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  131,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  132,   -1,   -1,  133,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       134,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       135,  136,   -1,  137,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  138,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  139,   -1,   -1,   -1,
        -1,   -1,   -1,  140,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  141,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  142,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  143,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  144,   -1,   -1,   -1,   -1,
        -1,  145,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  146,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       147,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  148
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
          else if (index < -TOTAL_KEYWORDS)
            {
              register int offset = - 1 - TOTAL_KEYWORDS - index;
              register const struct Color *wordptr = &wordlist[TOTAL_KEYWORDS + lookup[offset]];
              register const struct Color *wordendptr = wordptr + -lookup[offset + 1];

              while (wordptr < wordendptr)
                {
                  register const char *s = wordptr->name;

                  if (*str == *s && !strcmp (str + 1, s + 1))
                    return wordptr;
                  wordptr++;
                }
            }
        }
    }
  return 0;
}
