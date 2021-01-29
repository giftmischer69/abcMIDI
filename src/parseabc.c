/* 
 * parseabc.c - code to parse an abc file. This file is used by the
 * following 3 programs :
 * abc2midi - program to convert abc files to MIDI files.
 * abc2abc  - program to manipulate abc files.
 * yaps     - program to convert abc to PostScript music files.
 * Copyright (C) 1999 James Allwright
 * e-mail: J.R.Allwright@westminster.ac.uk
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


/* Macintosh port 30th July 1996 */
/* DropShell integration   27th Jan  1997 */
/* Wil Macaulay (wil@syndesis.com) */ 


#define TAB 9
#include "abc.h"
#include "parseabc.h"
#include <stdio.h>

#define SIZE_ABBREVIATIONS ('Z' - 'H' + 1)

#ifdef __MWERKS__
#define __MACINTOSH__ 1
#endif /* __MWERKS__ */

#ifdef __MACINTOSH__
#define main macabc2midi_main
#define STRCHR
#endif /* __MACINTOSH__ */

/* define USE_INDEX if your C libraries have index() instead of strchr() */
#ifdef USE_INDEX
#define strchr index
#endif

#ifdef ANSILIBS
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#else
extern char* malloc();
extern char* strchr();
#endif

int lineno;
static int parsing, slur;
static int inhead, inbody;
static int parserinchord;
int chorddecorators[DECSIZE];
char decorations[] = ".MLRH~Tuv";
static char *abbreviation[SIZE_ABBREVIATIONS];

int* checkmalloc(bytes)
/* malloc with error checking */
int bytes;
{
  int *p;

  p = (int*) malloc(bytes);
  if (p == NULL) {
    printf("Out of memory error - malloc failed!\n");
    exit(0);
  };
  return (p);
}

char* addstring(s)
/* create space for string and store it in memory */
char* s;
{
  char* p;

  p = (char*) checkmalloc(strlen(s)+1);
  strcpy(p, s);
  return(p);
}

void initvstring(s)
struct vstring *s;
/* initialize vstring (variable length string data structure) */
{
  s->len = 0;
  s->limit = 40;
  s->st = (char*) checkmalloc(s->limit + 1);
  *(s->st) = '\0';
}

void extendvstring(s)
struct vstring *s;
/* doubles character space available in string */
{
  char* p;

  if (s->limit > 0) {
    s->limit = s->limit * 2;
    p = (char*) checkmalloc(s->limit + 1);
    strcpy(p, s->st);
    free(s->st);
    s->st = p;
  } else {
    initvstring(s);
  };
}

void addch(ch, s)
char ch;
struct vstring *s;
/* appends character to vstring structure */
{
  if (s->len >= s->limit) {
    extendvstring(s);
  };
  *(s->st+s->len) = ch;
  *(s->st+(s->len)+1) = '\0';
  s->len = (s->len) + 1;
}

void addtext(text, s)
char* text;
struct vstring *s;
/* appends a string to vstring data structure */
{
  int newlen;

  newlen = s->len + strlen(text);
  while (newlen >= s->limit) {
    extendvstring(s);
  };
  strcpy(s->st+s->len, text);
  s->len = newlen;
}

void clearvstring(s)
struct vstring *s;
/* set string to empty */
/* does not deallocate memory ! */
{
  *(s->st) = '\0';
  s->len = 0;
}

void freevstring(s)
struct vstring *s;
/* deallocates memory allocated for string */
{
  if (s->st != NULL) {
    free(s->st);
    s->st = NULL;
  };
  s->len = 0;
  s->limit = 0;
}

void parseron()
{
  parsing = 1;
  slur = 0;
}

void parseroff()
{
  parsing = 0;
  slur = 0;
}

int getarg(option, argc, argv)
/* look for argument 'option' in command line */
char *option;
char *argv[];
int argc;
{
  int j, place;

  place = -1;
  for (j=0; j<argc; j++) {
    if (strcmp(option, argv[j]) == 0) {
      place = j + 1;
    };
  };
  return (place);
}

void skipspace(p)
char **p;
{
  /* skip space and tab */
  while(((int)**p == ' ') || ((int)**p == TAB)) *p = *p + 1;
}

int readnumf(num)
char *num;
/* read integer from string without advancing character pointer */
{
  int t;
  char* p;

  p =num;
  if (!isdigit(*p)) {
    event_error("Missing Number");
  };
  t = 0;
  while (((int)*p >= '0') && ((int)*p <= '9')) {
    t = t * 10 + (int) *p - '0';
    p = p + 1;
  };
  return (t);
}

int readsnumf(s)
char* s;
/* reads signed integer from string without advancing character pointer */
{
  char* p;

  p = s;
  if (*p == '-') {
    p = p+1;
    skipspace(&p);
    return(-readnumf(p));
  } else {
    return(readnumf(p));
  }
}

int readnump(p)
char **p;
/* read integer from string and advance character pointer */
{
  int t;

  t = 0;
  while (((int)**p >= '0') && ((int)**p <= '9')) {
    t = t * 10 + (int) **p - '0';
    *p = *p + 1;
  };
  return (t);
}

int readsnump(p)
char** p;
/* reads signed integer from string and advance character pointer */
{
  if (**p == '-') {
    *p = *p+1;
    skipspace(p);
    return(-readnump(p));
  } else {
    return(readnump(p));
  }
}

void readsig(a, b, sig)
int *a, *b;
char **sig;
/* read time signature (meter) from M: field */
{
  int t;

  if ((**sig == 'C') || (**sig == 'c')) {
    *a = 4;
    *b = 4;
    return;
  };
  *a = readnump(sig);
  if ((int)**sig != '/') {
    event_error("Missing / ");
  } else {
    *sig = *sig + 1;
  };
  *b = readnump(sig);
  if ((*a == 0) || (*b == 0)) {
    event_error("Expecting fraction in form A/B");
  } else {
    t = *b;
    while (t > 1) {
      if (t%2 != 0) {
        event_error("divisor must be a power of 2");
        t = 1;
        *b = 0;
      } else {
        t = t/2;
      };
    };
  };
}

void readlen(a, b, p)
int *a, *b;
char **p;
/* read length part of a note and advance character pointer */
{
  int t;

  *a = readnump(p);
  if (*a == 0) {
    *a = 1;
  };
  *b = 1;
  if (**p == '/') {
    *p = *p + 1;
    *b = readnump(p);
    if (*b == 0) {
      *b = 2;
      while (**p == '/') {
        *b = *b * 2;
        *p = *p + 1;
      };
    };
  };
  t = *b;
  while (t > 1) {
    if (t%2 != 0) {
      event_warning("divisor not a power of 2");
      t = 1;
    } else {
      t = t/2;
    };
  };
}


int isclef(s)
char* s;
/* part of K: parsing - looks for a clef in K: field                 */
/* format is K:string where string is treble, bass, baritone, tenor, */
/* alto, mezzo, soprano or K:clef=arbitrary                          */
{
  int gotclef;

  s = s;
  gotclef = 0;
  if (strncmp(s, "bass", 4) == 0) {
    gotclef= 1;
  };
  if (strncmp(s, "treble", 6) == 0) {
    gotclef= 1;
  };
  if (strncmp(s, "baritone", 8) == 0) {
    gotclef= 1;
  };
  if (strncmp(s, "tenor", 5) == 0) {
    gotclef= 1;
  };
  if (strncmp(s, "alto", 4) == 0) {
    gotclef= 1;
  };
  if (strncmp(s, "mezzo", 5) == 0) {
    gotclef= 1;
  };
  if (strncmp(s, "soprano", 7) == 0) {
    gotclef= 1;
  };
  return(gotclef);
}

char* readword(word, s)
/* part of parsekey, extracts word from input line */
char word[];
char* s;
{
  char* p;
  int i;

  p = s;
  i = 0;
  while ((*p != '\0') && (*p != ' ') && ((i == 0) || (*p != '='))) {
    if (i < 29) {
      word[i] = *p;
      i = i + 1;
    };
    p = p + 1;
  };
  word[i] = '\0';
  return(p);
}

static void lcase(s)
/* convert word to lower case */
char* s;
{
  char* p;

  p = s;
  while (*p != '\0') {
    if (isupper(*p)) {
      *p = *p + 'a' - 'A';
    };
    p = p + 1;
  };
}

static int casecmp(s1, s2)
/* case-insensitive compare 2 strings */
/* return 0 if equal   */
/*        1 if s1 > s2 */
/*       -1 if s1 > s2 */
char s1[];
char s2[];
{
  int i, val, done;
  char c1, c2;

  i = 0;
  done = 0;
  while (done == 0) {
    c1 = tolower(s1[i]);
    c2 = tolower(s2[i]);
    if (c1 > c2) {
      val = 1;
      done = 1;
    } else {
      if (c1 < c2) {
        val = -1;
        done = 1;
      } else {
        if (c1 == '\0') {
          val = 0;
          done = 1;
        } else {
          i = i + 1;
        };
      };
    };
  };
  return(val);
}

int parsekey(str)
/* parse contents of K: field */
/* this works by picking up a strings and trying to parse them */
/* returns 1 if valid key signature found, 0 otherwise */
char* str;
{
  char* s;
  char word[30];
  int parsed;
  int gotclef, gotkey, gotoctave, gottranspose;
  int foundmode;
  int transpose, octave;
  char clefstr[30];
  char modestr[30];
  char msg[80];
  char* moveon;
  int sf, minor;
  char modmap[7];
  int modmul[7];
  int i, j;
  static char *key = "FCGDAEB";
  static char *mode[10] = {"maj", "min", "m", 
                       "aeo", "loc", "ion", "dor", "phr", "lyd", "mix"};
  static int modeshift[10] = {0, -3, -3,
                         -3, -5, 0, -2, -4, 1, -1 };
  static int modeminor[10] = {0, 1, 1,
                          1, 0, 0, 0, 0, 0, 0};

  s = str;
  octave = 0;
  transpose = 0;
  gotkey = 0;
  gotclef = 0;
  gotoctave = 0;
  gottranspose = 0;
  for (i=0; i<7; i++) {
    modmap[i] = ' ';
    modmul[i] = 1;
  };
  while (*s != '\0') {
    skipspace(&s);
    s = readword(word, s);
    parsed = 0;
    if (casecmp(word, "clef") == 0) {
      skipspace(&s);
      if (*s != '=') {
        event_error("clef must be followed by '='");
      } else {
        s = s + 1;
        skipspace(&s);
        s = readword(clefstr, s);
        if (strlen(clefstr) > 0) {
          gotclef = 1;
        };
      };
      parsed = 1;
    };
    if ((parsed == 0) && (casecmp(word, "transpose") == 0)) {
      skipspace(&s);
      if (*s != '=') {
        event_error("transpose must be followed by '='");
      } else {
        s = s + 1;
        skipspace(&s);
        transpose = readsnump(&s);
        gottranspose = 1;
      };
      parsed = 1;
    };
    if ((parsed == 0) && (casecmp(word, "octave") == 0)) {
      skipspace(&s);
      if (*s != '=') {
        event_error("octave must be followed by '='");
      } else {
        s = s + 1;
        skipspace(&s);
        octave = readsnump(&s);
        gotoctave = 1;
      };
      parsed = 1;
    };
    if ((parsed == 0) && (isclef(word))) {
      gotclef = 1;
      strcpy(clefstr, word);
      parsed = 1;
    };
    if ((parsed == 0) && (casecmp(word, "Hp") == 0)) {
      sf = 2;
      minor = 0;
      gotkey = 1;
      parsed = 1;
    };
    if ((parsed == 0) && ((word[0] >= 'A') && (word[0] <= 'G'))) {

      gotkey = 1;
      parsed = 1;
      /* parse key itself */
      sf = (int) strchr(key, word[0]) - (int) &key[0] - 1;
      j = 1;
      /* deal with sharp/flat */
      if (word[1] == '#') {
        sf += 7;
        j = 2;
      } else {
        if (word[1] == 'b') {
          sf -= 7;
          j = 2;
        };
      }
      minor = 0;
      foundmode = 0;
      if (strlen(word) == j) {
        /* look at next word for mode */
        skipspace(&s);
        moveon = readword(modestr, s);
        lcase(modestr);
        for (i = 0; i<10; i++) {
          if (strncmp(modestr, mode[i], 3) == 0) {
            foundmode = 1;
            sf = sf + modeshift[i];
            minor = modeminor[i];
          };
        };
        if (foundmode) {
          s = moveon;
        };
      } else {
        strcpy(modestr, &word[j]);
        lcase(modestr);
        for (i = 0; i<10; i++) {
          if (strncmp(modestr, mode[i], 3) == 0) {
            foundmode = 1;
            sf = sf + modeshift[i];
            minor = modeminor[i];
          };
        };
        if (!foundmode) {
          sprintf(msg, "Unknown mode '%s'", &word[j]);
          event_error(msg);
        };
      };
    };
    if (gotkey) {
      if (sf > 7) {
        event_warning("Unusual key representation");
       sf = sf - 12;
      } ;
      if (sf < -7) {
        event_warning("Unusual key representation");
        sf = sf + 12;
      };
    };
    if ((word[0] == '^') || (word[0] == '_') || (word[0] == '=')) {
      if ((strlen(word) == 2) && (word[1] >= 'a') && (word[1] <= 'g')) {
        j = (int)word[1] - 'a';
        modmap[j] = word[0];
        modmul[j] = 1;
        parsed = 1;
      } else {
        if ((strlen(word) == 3) && (word[0] != '=') && (word[0] == word[1]) &&
            (word[2] >= 'a') && (word[2] <= 'g')) {
          j = (int)word[2] - 'a';
          modmap[j] = word[0];
          modmul[j] = 2;
          parsed = 1;
        };
      };
    };
    if ((parsed == 0) && (strlen(word) > 0)) {
      sprintf(msg, "Ignoring string '%s' in K: field", word);
      event_warning(msg);
    };
  };
  event_key(sf, str, minor, modmap, modmul, gotkey, gotclef, clefstr,
            octave, transpose, gotoctave, gottranspose);
  return(gotkey);
}

static void parsenote(s)
char **s;
/* parse abc note and advance character pointer */
{
  int decorators[DECSIZE];
  int i, t;
  int mult;
  char accidental, note;
  int octave, n, m;
  char msg[80];

  mult = 1;
  accidental = ' ';
  note = ' ';
  for (i = 0; i<DECSIZE; i++) decorators[i] = 0;
  while (strchr(decorations, **s) != NULL) {
    t = (int) strchr(decorations, **s) -  (int) decorations;
    decorators[t] = 1;
    *s = *s + 1;
  };
  /*check for decorated chord */
  if (**s == '[') {
    event_warning("decorations applied to chord");
    for (i = 0; i<DECSIZE; i++) chorddecorators[i] = decorators[i];
    event_chordon();
    parserinchord = 1;
    *s = *s + 1;
    skipspace(s);
  };
  if (parserinchord) {
    /* inherit decorators */
    for (i = 0; i<DECSIZE; i++) {
      decorators[i] = decorators[i] | chorddecorators[i];
    };
  };
  /* read accidental */
  switch (**s) {
  case '_':
    accidental = **s;
    *s = *s + 1;
    if (**s == '_') {
      *s = *s + 1;
      mult = 2;
    };
    break;
  case '^':
    accidental = **s;
    *s = *s + 1;
    if (**s == '^') {
      *s = *s + 1;
      mult = 2;
    };
    break;
  case '=':
    accidental = **s;
    *s = *s + 1;
    if ((**s == '^') || (**s == '_')) {
      accidental = **s;
    };
    break;
  default:
    /* do nothing */
    break;
  };
  if ((**s >= 'a') && (**s <= 'g')) {
    note = **s;
    octave = 1;
    *s = *s + 1;
    while ((**s == '\'') || (**s == ',')) {
      if (**s == '\'') {
        octave = octave + 1;
        *s = *s + 1;
      };
      if (**s == ',') {
        sprintf(msg, "Bad pitch specifier , after note %c", note);
        event_error(msg);
        octave = octave - 1;
        *s = *s + 1;
      };
    };
  } else {
    if ((**s >= 'A') && (**s <= 'G')) {
      note = **s + 'a' - 'A';
      octave = 0;
      *s = *s + 1;
      while ((**s == '\'') || (**s == ',')) {
        if (**s == ',') {
          octave = octave - 1;
          *s = *s + 1;
        };
        if (**s == '\'') {
          sprintf(msg, "Bad pitch specifier ' after note %c", note + 'A' - 'a');
          event_error(msg);
          octave = octave + 1;
          *s = *s + 1;
        };
      };
    };
  };
  if (note == ' ') {
    event_error("Malformed note : expecting a-g or A-G");
  } else {
    readlen(&n, &m, s);
    event_note(decorators, accidental, mult, note, octave, n, m);
  };
}

char* getrep(p, out)
char* p;
char* out;
/* look for number or list following [ | or :| */
{
  char* q;
  int digits;
  int done;
  int count;

  q = p;
  count = 0;
  done = 0;
  digits = 0;
  while (!done) {
    if (isdigit(*q)) {
      out[count] = *q;
      count = count + 1;
      q = q + 1;
      digits = digits + 1;
    } else {
      if (((*q == '-')||(*q == ','))&&(digits > 0)&&(isdigit(*(q+1)))) {
        out[count] = *q;
        count = count + 1;
        q = q + 1;
        digits = 0;
      } else {
        done = 1;
      };
    };
  };
  out[count] = '\0';
  return(q);
}

int checkend(s)
char* s;
/* returns 1 if we are at the end of the line 0 otherwise */
/* used when we encounter '\' '*' or other special line end characters */
{
  char* p;
  int atend;

  p = s;
  skipspace(&p);
  if (*p == '\0') {
    atend = 1;
  } else {
    atend = 0;
  };
  return(atend);
}

void readstr(out, in, limit)
char out[];
char **in;
int limit;
/* copy across alphanumeric string */
{
  int i;

  i = 0;
  while ((isalpha(**in)) && (i < limit-1)) {
    out[i] = **in;
    i = i + 1;
    *in = *in + 1;
  };
  out[i] = '\0';
}

static void parse_precomment(s)
char* s;
/* handles a comment field */
{
  char package[40];
  char *p;

  if (*s == '%') {
    p = s+1;
    readstr(package, &p, 40);
    event_specific(package, p);
  } else {
    event_comment(s);
  };
}

static void parse_tempo(place)
char* place;
/* parse tempo descriptor i.e. Q: field */
{
  char* p;
  int a, b;
  int n;
  int relative;
  char *pre_string;
  char *post_string;

  relative = 0;
  p = place;
  pre_string = NULL;
  if (*p == '"') {
    p = p + 1;
    pre_string = p;
    while ((*p != '"') && (*p != '\0')) {
      p = p + 1;
    };
    if (*p == '\0') {
      event_error("Missing closing double quote");
    } else {
      *p = '\0';
      p = p + 1;
      place = p;
    };
  };
  while ((*p != '\0') && (*p != '=')) p = p + 1;
  if (*p == '=') {
    p = place;
    skipspace(&p);
    if (((*p >= 'A') && (*p <= 'G')) || ((*p >= 'a') && (*p <= 'g'))) {
      relative = 1;
      p = p + 1;
    };
    readlen(&a, &b, &p);
    skipspace(&p);
    if (*p != '=') {
      event_error("Expecting = in tempo");
    };
    p = p + 1;
  } else {
    a = 0;
    b = 0;
    p = place;
  };
  skipspace(&p);
  n = readnump(&p);
  post_string = NULL;
  if (*p == '"') {
    p = p + 1;
    post_string = p;
    while ((*p != '"') && (*p != '\0')) {
      p = p + 1;
    };
    if (*p == '\0') {
      event_error("Missing closing double quote");
    } else {
      *p = '\0';
      p = p + 1;
    };
  };
  event_tempo(n, a, b, relative, pre_string, post_string);
}

preparse_words(s)
char *s;
/* takes a line of lyrics (w: field) and strips off */
/* any continuation character */
{
  int continuation;
  int l;

  /* printf("Parsing %s\n", s); */
  /* strip off any trailing spaces */
  l = strlen(s) - 1;
  while ((l>= 0) && (*(s+l) == ' ')) {
    *(s+l) = '\0';
    l = l - 1;
  };
  if (*(s+l) != '\\') {
    continuation = 0;
  } else {
    continuation = 1;
    /* remove continuation character */
    *(s+l) = '\0';
    l = l - 1;
    while ((l>= 0) && (*(s+l) == ' ')) {
      *(s+l) = '\0';
      l = l - 1;
    };
  };
  event_words(s, continuation);
}

static void init_abbreviations()
/* initialize mapping of H-Z to strings */
{
  int i;

  for (i = 0; i< 'Z' - 'H'; i++) {
     abbreviation[i] = NULL;
  };
}

static void record_abbreviation(char symbol, char *string)
/* update record of abbreviations when a U: field is encountered */
{
  int index;

  if ((symbol <'H') || (symbol > 'Z')) {
    return;
  };
  index = symbol - 'H';
  if (abbreviation[index] != NULL) {
    free(abbreviation[index]);
  };
  abbreviation[index] = addstring(string);
}

char *lookup_abbreviation(char symbol)
/* return string which s abbreviates */
{
  if ((symbol < 'H') || (symbol > 'Z')) {
    return(NULL);
  } else {
    return(abbreviation[symbol - 'H']);
  };
}

static void free_abbreviations()
/* free up any space taken by abbreviations */
{
  int i;

  for (i=0; i<SIZE_ABBREVIATIONS; i++) {
    if (abbreviation[i] != NULL) {
      free(abbreviation[i]);
    };
  };
}

static void parsefield(key, field)
char key;
char* field;
/* top-level routine handling all lines containing a field */
{
  char* comment;
  char* place;
  int iscomment;
  int foundkey;

  if ((inbody) && (strchr("EIKLMPQTVwW", key) == NULL)) {
    event_error("Field not allowed in tune body");
  };
  comment = field;
  iscomment = 0;
  while ((*comment != '\0') && (*comment != '%')) {
    comment = comment + 1;
  };
  if (*comment == '%') {
    iscomment = 1;
    *comment = '\0';
    comment = comment + 1;
  };
  place =field;
  skipspace(&place);
  switch (key) {
  case 'X':
    {
      int x;
    
      x = readnumf(place);
      if (inhead) {
        event_error("second X: field in header");
      };
      event_refno(x);
      inhead = 1;
      inbody = 0;
      parserinchord = 0;
      break;
    };
  case 'K':
    foundkey = parsekey(place);
    if (inhead || inbody) {
      if (foundkey) {
        inbody = 1;
        inhead = 0;
      } else {
        if (inhead) {
          event_error("First K: field must specify key signature");
        };
      };
    } else {
      event_error("No X: field preceding K:");
    };
    break;
  case 'M':
    {
      int num, denom;

      if (strncmp(place, "none", 4) == 0) {
        event_timesig(4, 4, 0);
      } else {
        readsig(&num, &denom, &place);
        if ((*place == 's') || (*place == 'l')) {
          event_error("s and l in M: field not supported");
        };
        if ((num != 0) && (denom != 0)) {
          event_timesig(num, denom, 1);
        };
      };
      break;
    };
  case 'L':
    {
      int num, denom;

      readsig(&num, &denom, &place);
      if (num != 1) {
        event_error("Default length must be 1/X");
      } else {
        if (denom > 0) {
          event_length(denom);
        } else {
          event_error("invalid denominator");
        };
      };
      break;
    };
  case 'P':
    event_part(place);
    break;
  case 'I':
    event_info(place);
    break;
  case 'V':
    {
      int num;

      skipspace(&place);
      if ((*place >= '0') && (*place <= '9')) {
        num = readnump(&place);
      } else {
        num = 0;
        event_error("No voice number in V: field");
      };
      skipspace(&place);
      event_voice(num, place);
      break;
    };
  case 'Q':
    parse_tempo(place);
    break;
  case 'U':
    {
      char symbol;
      char container;
      char *expansion;

      skipspace(&place);
      if ((*place >= 'H') && (*place <= 'Z')) {
        symbol = *place;
        place = place + 1;
        skipspace(&place);
        if (*place == '=') {
          place = place + 1;
          skipspace(&place);
          if (*place == '!') {
            place = place + 1;
            container = '!';
            expansion = place;
            while ((!iscntrl(*place)) && (*place != '!')) {
              place = place +1;
            };
            if (*place != '!') {
              event_error("No closing ! in U: field");
            };
            *place = '\0';
          } else {
            container = ' ';
            expansion = place;
            while (isalnum(*place)) {
              place = place + 1;
            };
            *place = '\0';
          };
          if (strlen(expansion) > 0) {
            record_abbreviation(symbol, expansion);
            event_abbreviation(symbol, expansion, container);
          } else {
            event_error("Missing term in U: field");
          };
        } else {
          event_error("Missing '=' U: field ignored");
        };
      } else {
        event_warning("only 'H' - 'Z' supported in U: field");
      };
    };
    break;
  case 'w':
    preparse_words(place);
    break;
  default:
    event_field(key, place);
  };
  if (iscomment) {
    parse_precomment(comment);  
  };
}

char* parseinlinefield(p)
char* p;
/* parse field within abc line e.g. [K:G] */
{
  char* q;

  event_startinline();
  q = p;
  while ((*q != ']') && (*q != '\0')) {
    q = q + 1;
  };
  if (*q == ']') {
    *q = '\0';
    parsefield(*p, p+2);
    q = q + 1;
  } else {
    event_error("missing closing ]");
    parsefield(*p, p+2);
  };
  event_closeinline();
  return(q);
}

static void parsemusic(field)
char* field;
/* parse a line of abc notes */
{
  char* p;
  char* comment;
  char endchar;
  int iscomment;
  int starcount;
  int i;
  char playonrep_list[80];

  event_startmusicline();
  endchar = ' ';
  comment = field;
  iscomment = 0;
  while ((*comment != '\0') && (*comment != '%')) {
    comment = comment + 1;
  };
  if (*comment == '%') {
    iscomment = 1;
    *comment = '\0';
    comment = comment + 1;
  };

  p = field;
  skipspace(&p);
  while(*p != '\0') {
    if (((*p >= 'a') && (*p <= 'g')) || ((*p >= 'A') && (*p <= 'G')) ||
        (strchr("_^=", *p) != NULL) || (strchr(decorations, *p) != NULL)) {
      parsenote(&p);
    } else {
      switch(*p) {
      case '+':
        event_chord();
        parserinchord = 1 - parserinchord;
        if (parserinchord == 0) {
          for (i = 0; i<DECSIZE; i++) chorddecorators[i] = 0;
        };
        p = p + 1;
        break;
      case '"':
        {
          struct vstring gchord;
   
          p = p + 1;
          initvstring(&gchord);
          while ((*p != '"') && (*p != '\0')) {
            addch(*p, &gchord);
            p = p + 1;
          };
          if (*p == '\0') {
            event_error("Guitar chord name not properly closed");
          } else {
            p = p + 1;
          };
          event_gchord(gchord.st);
          freevstring(&gchord);
          break;
        };
      case '|':
        p = p + 1;
        switch(*p) {
          case ':':
            event_bar(BAR_REP, "");
            p = p + 1;
            break;
          case '|' :
            event_bar(DOUBLE_BAR, "");
            p = p + 1;
            break;
          case ']' :
            event_bar(THIN_THICK, "");
            p = p + 1;
            break;
          default :
            p = getrep(p, playonrep_list);
            event_bar(SINGLE_BAR, playonrep_list);
        };
        break;
      case ':':
        p = p + 1;
        switch(*p) {
          case ':':
            event_bar(DOUBLE_REP, "");
            p = p + 1;
            break;
          case '|':
            p = p + 1;
            p = getrep(p, playonrep_list);
            event_bar(REP_BAR, playonrep_list);
            break;
          default:
            event_error("Single colon in bar");
        };
        break;
      case ' ':
        event_space();
        skipspace(&p);
        break;
      case TAB:
        event_space();
        skipspace(&p);
        break;
      case '(':
        p = p + 1;
        {
          int t, q, r;

          t = 0;
          q = 0;
          r = 0;
          t = readnump(&p);
          if ((t != 0) && (*p == ':')) {
            p = p + 1;
            q = readnump(&p);
            if (*p == ':') {
              p = p + 1;
              r = readnump(&p);
            };
          };
          if (t == 0) {
            if (slur > 0) {
              event_warning("Slur within slur");
            };
            slur = slur + 1;
            event_sluron(slur);
          } else {
            event_tuple(t, q, r);
          };
        };
        break;
      case ')':
        p = p + 1;
        if (slur == 0) {
          event_error("No slur to close");
        } else {
          slur = slur - 1;
        };
        event_sluroff(slur);
        break;
      case '{':
        p = p + 1;
        event_graceon();
        break;
      case '}':
        p = p + 1;
        event_graceoff();
        break;
      case '[':
        p = p + 1;
        switch(*p) {
/* following lines are now redundant */
/*
        case '1':
          p = p + 1;
          event_rep1();
          break;
        case '2':
          p = p + 1;
          event_rep2();
          break;
*/
        case '|':
          p = p + 1;
          event_bar(THICK_THIN, "");
          break;
        default:
          if (isdigit(*p)) {
            p = getrep(p, playonrep_list);
            event_playonrep(playonrep_list);
          } else {
            if (isalpha(*p) && (*(p+1) == ':')) {
              p = parseinlinefield(p);
            } else {
              event_chordon();
              parserinchord = 1;
            };
          };
          break;
        };
        break;
      case ']':
        p = p + 1;
        event_chordoff();
        parserinchord = 0;
        for (i = 0; i<DECSIZE; i++) chorddecorators[i] = 0;
        break;
      case 'z':
        {
          int n, m;

          p = p + 1;
          readlen(&n, &m, &p);
          event_rest(n, m);
          break;
        };
      case 'Z':
        {
          int n, m;

          p = p + 1;
          readlen(&n, &m, &p);
          if (m != 1) {
            event_error("Z must be followed by a whole integer");
          };
          event_mrest(n, m);
          break;
        };
      case '>':
        {
          int n;

          n = 0;
          while (*p == '>') {
            n = n + 1;
            p = p + 1;
          };
          if (n>3) {
            event_error("Too many >'s");
          } else {
            event_broken(GT, n);
          };
          break;
        };
      case '<':
        {
          int n;

          n = 0;
          while (*p == '<') {
            n = n + 1;
            p = p + 1;
          };
          if (n>3) {
            event_error("Too many <'s");
          } else {
            event_broken(LT, n);
          };
          break;
        };
      case 's':
        if (slur == 0) {
          slur = 1;
        } else {
          slur = slur - 1;
        };
        event_slur(slur);
        p = p + 1;
        break;
      case '-':
        event_tie();
        p = p + 1;
        break;
      case '\\':
        p = p + 1;
        if (checkend(p)) {
          event_lineend('\\', 1);
          endchar = '\\';
        } else {
          event_error("'\\' in middle of line ignored");
        };
        break;
      case '!':
        {
          struct vstring instruction;
          char *s;
   
          p = p + 1;
          s = p;
          initvstring(&instruction);
          while ((*p != '!') && (*p != '\0')) {
            addch(*p, &instruction);
            p = p + 1;
          };
          if (*p != '!') {
            p = s;
            if (checkend(s)) {
              event_lineend('!', 1);
              endchar = '!';
            } else {
              event_error("'!' in middle of line ignored");
            };
          } else {
            event_instruction(instruction.st);
            p = p + 1;
          };
          freevstring(&instruction);
        };
        break;
      case '*':
        p = p + 1;
        starcount = 1;
        while (*p == '*') {
          p = p + 1;
          starcount = starcount + 1;
        };
        if (checkend(p)) {
          event_lineend('*', starcount);
          endchar = '*';
        } else {
          event_error("*'s in middle of line ignored");
        };
        break;
      default:
        {
          char msg[40];

          if ((*p >= 'H') && (*p <= 'Z')) {
            event_reserved(*p);
          } else {
            sprintf(msg, "Unrecognized character: %c", *p);
            event_error(msg);
          };
        };
        p = p + 1;
      };
    };
  };
  event_endmusicline(endchar);
  if (iscomment) {
    parse_precomment(comment);
  };
}

static void parseline(line)
char* line;
/* top-level routine for handling a line in abc file */
{
  char *p, *q;

  /* printf("%d parsing : %s\n", lineno, line); */
  p = line;
  skipspace(&p);
  if (strlen(p) == 0) {
    event_blankline();
    inhead = 0;
    inbody = 0;
    return;
  };
  if ((int)*p == '\\') {
    if (parsing) {
      event_tex(p);
    };
    return;
  };
  if ((int)*p == '%') {
    parse_precomment(p+1);
    return;
  };
  if (strchr("ABCDEFGHIKLMNOPQRSTUVwWXZ", *p) != NULL) {
    q = p + 1;
    skipspace(&q);
    if ((int)*q == ':') {
      if (*(line+1) != ':') {
        event_warning("whitespace in field declaration");
      };
      if ((*(q+1) == ':') || (*(q+1) == '|')) {
        event_warning("potentially ambiguous line");
      };
      parsefield(*p, q+1);
    } else {
      if (inbody) {
        if (parsing) parsemusic(p);
      } else {
        event_text(p);
      };
    };
  } else {
    if (inbody) {
      if (parsing) parsemusic(p);
    } else {
      event_text(p);
    };
  };
}

static void parsefile(name)
char* name;
/* top-level routine for parsing file */
{
  FILE *fp;
  int reading;
  int fileline;
  struct vstring line;
  /* char line[MAXLINE]; */
  int t;
  int lastch, done_eol;

  /* printf("parsefile called %s\n", name); */
  /* The following code permits abc2midi to read abc from stdin */
  if ((strcmp(name, "stdin") == 0) || (strcmp(name, "-") == 0)) {
    fp = stdin;
  } else {
    fp = fopen(name, "r");
  };
  if (fp == NULL) {
    printf("Failed to open file %s\n", name);
    exit(1);
  };
  inhead = 0;
  inbody = 0;
  parseroff();
  reading = 1;
  line.limit = 4;
  initvstring(&line);
  fileline = 1;
  done_eol = 0;
  lastch = '\0';
  while (reading) {
    t = getc(fp);
    if (t == EOF) {
      reading = 0;
      if (line.len>0) {
        parseline(line.st);
        fileline = fileline + 1;
        lineno = fileline;
        event_linebreak();
      };
    } else {
      /* recognize  \n  or  \r  or  \r\n  or  \n\r  as end of line */
      /* should work for DOS, unix and Mac files */
      if ((t != '\n') && (t != '\r')) {
        addch((char) t, &line);
        done_eol = 0;
      } else {
        if ((done_eol) && (((t == '\n') && (lastch == '\r')) || 
                           ((t == '\r') && (lastch == '\n')))) {
          done_eol = 0;
          /* skip this character */
        } else {
          /* reached end of line */
          parseline(line.st);
          clearvstring(&line);
          fileline = fileline + 1;
          lineno = fileline;
          event_linebreak();
          done_eol = 1;
        };
      };
      lastch = t;
    };
  };
  fclose(fp);
  event_eof();
  freevstring(&line);
}    

int main(argc,argv)
int argc;
char *argv[];
{
  char *filename;

  event_init(argc, argv, &filename);
  if (argc < 2) {
    /* printf("argc = %d\n", argc); */
  } else {
    init_abbreviations();
    parsefile(filename);
    free_abbreviations();
  };
  return(0);
}

/*
int getline ()
{
  return (lineno);
}
*/
