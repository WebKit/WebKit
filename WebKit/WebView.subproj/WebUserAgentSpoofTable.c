/* ANSI-C code produced by gperf version 2.7.2 */
/* Command-line: gperf -CEot -L ANSI-C -k '*' -N _web_findSpoofTableEntry -F ,0 WebView.subproj/WebUserAgentSpoofTable.gperf  */
struct UserAgentSpoofTableEntry { const char *name; UserAgentStringType type; };
/* maximum key range = 27, duplicates = 0 */

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
  static const unsigned char asso_values[] =
    {
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36,  5,  0, 36, 36, 15,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36,  0,  0,  0,
       0,  0, 10,  0,  0,  0, 10, 36,  0,  0,
       0,  0,  0, 36,  0,  0,  0,  0,  5,  0,
      36, 15,  0, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36
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
const struct UserAgentSpoofTableEntry *
_web_findSpoofTableEntry (register const char *str, register unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 18,
      MIN_WORD_LENGTH = 6,
      MAX_WORD_LENGTH = 20,
      MIN_HASH_VALUE = 9,
      MAX_HASH_VALUE = 35
    };

  static const struct UserAgentSpoofTableEntry wordlist[] =
    {
      {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0}, {"",0},
      {"",0}, {"",0},
      {"watch.com", MacIE},
      {"battle.net", MacIE},
      {"porsche.com", MacIE},
      {"mazdausa.com", MacIE},
      {"hondacars.com", MacIE},
      {"abcnews.go.com", WinIE},
      {"",0},
      {"nj.com", MacIE},
      {"",0},
      {"hondaredriders.com", MacIE},
      {"oregonlive.com", MacIE},
      {"jaguar.com", MacIE},
      {"freebsd.org", MacIE},
      {"firstusa.com", MacIE},
      {"microsoft.com", MacIE},
      {"pier1.com", MacIE},
      {"",0},
      {"olympic.org", MacIE},
      {"",0},
      {"disney.go.com", MacIE},
      {"",0}, {"",0},
      {"bang-olufsen.com", MacIE},
      {"",0}, {"",0}, {"",0},
      {"wap.sonyericsson.com", MacIE}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
