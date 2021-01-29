/*
 * toabc.c - part of abc2abc - program to manipulate abc files.
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

/* back-end for outputting (possibly modified) abc */

#include "abc.h"
#include "parseabc.h"
#include <stdio.h>

/* define USE_INDEX if your C libraries have index() instead of strchr() */
#ifdef USE_INDEX
#define strchr index
#endif

#ifdef ANSILIBS
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#else
extern char* strchr();
#endif

#define MAX_VOICES 30
/* should be plenty! */

struct fract {
  int num;
  int denom;
};
struct fract barlen; /* length of a bar as given by the time signature */
struct fract unitlen; /* unit length as given by the L: field */
struct fract count; /* length of bar so far */
struct fract tuplefactor; /* factor associated with a tuple (N */
struct fract breakpoint; /* used to break bar into beamed sets of notes */
int barno; /* number of bar within tune */
int newspacing; /* was -s option selected ? */
int barcheck, repcheck; /* indicate -b and -r options selected */
int echeck; /* was error-checking turned off ? (-e option) */
int newbreaks; /* was -n option selected ? */
int totalnotes, notecount; 
int bars_per_line; /* number supplied after -n option */
int barcount;
int expect_repeat;
int tuplenotes, barend;
int xinhead, xinbody; /* are we in head or body of abc tune ? */
int inmusic; /* are we in a line of notes (in the tune body) ? */
int startline, blankline;
int transpose; /* number of semitones to transpose by (-t option) */
struct fract lenfactor; /* fraction to scale note lengths; -v,-d options */
int newkey; /* key after transposition (expressed as no. of sharps) */
int lines; /* used by transposition */
int oldtable[7], newtable[7]; /* for handling transposition */
int inchord; /* are we in a chord [ ] ? */
int ingrace; /* are we in a grace note set { } ? */
int chordcount; /* number of notes or rests in current chord */
int inlinefield; /* boolean - are we in [<field>: ] ? */
int cleanup; /* boolean to indicate -u option (update notation) */
char tmp[2000]; /* buffer to hold abc output being assembled */
int output_on = 1;  /* if 0 suppress output */
int selected_voice = -1; /* no voice was selected */
int newrefnos; /* boolean for -X option (renumber X: fields) */
int newref; /* next new number for X: field */
 
struct voicetype { /* information needed for each voice */
  int number; /* voice number from V: field */
  int barcount;
  int foundbar;
  struct abctext* currentline;
  int bars_remaining;
  int bars_complete;
} voice[MAX_VOICES];
int voicecount, this_voice, next_voice;
enum abctype {field, bar, barline};
/* linestat is used by -n for deciding when to generate a newline */
enum linestattype {fresh, midmusic, endmusicline, postfield};
enum linestattype linestat; 
/* struct abctext is used to store abc lines for re-formatting (-n option) */
struct lyricwords{
  struct lyricwords* nextverse;
  char* words;
};  
struct abctext{ /* linked list used to store output before re-formatting */
  struct abctext* next;
  char* text;
  enum abctype type;
  int notes;
  struct lyricwords* lyrics;
};
struct abctext* head;
struct abctext* tail;

static int purgespace(p)
char* p;
/* if string p is empty or consists of spaces, set p to the empty string */
/* and return 1, otherwise return 0. Used to test tmp */
/* part of new linebreak option (-n) */
{
  int blank;
  char *s;

  blank = 1;
  s = p;
  while (*s != '\0') {
    if (*s != ' ') blank = 0;
    s = s + 1;
  };
  if (blank) {
    *p = '\0';
  };
  return(blank);
}

int zero_barcount(foundbar)
/* initialize bar counter for abctext elements */
/* part of new linebreak option (-n) */
int *foundbar;
{
  *foundbar = 0;
  return(0);
}

int new_barcount(type, foundbar, oldcount)
enum abctype type;
int *foundbar;
int oldcount;
/* work out whether we have reached the end of a bar in abctext elements */
/* and increment barcount if we have */
/* part of new linebreak option (-n) */
{
  int new_value;

  new_value = oldcount;
  if (type == bar) {
    *foundbar = 1;
  };
  if ((type == barline) && (*foundbar == 1)) {
    new_value = new_value + 1;
    *foundbar = 0;
  };
  return(new_value);
}


static void setline(t)
enum linestattype t;
/* generates newline, or continuation, or nothing at all */
/* part of new linebreak option (-n) */
{
  if ((t == fresh) && ((linestat == postfield) || (linestat == endmusicline))) {
    printf("\n");
  };
  if ((t == fresh) && (linestat == midmusic)) {
    printf("\\\n");
  };
  linestat = t;
}

static int flush_abctext(bars, termination)
int bars;
enum linestattype termination;
/* outputs up to the specified number of bars of stored music */
/* and frees up the storage allocated for those bars. */
/* returns the number of bars actually output */
/* part of new linebreak option (-n) */
{
  struct abctext *p, *nextp;
  struct lyricwords *q, *r;
  struct lyricwords *barlyrics;
  int count, donewords, wordline;
  int i, foundtext;
  int foundbar;

  /* printf("flush_abctext called\n"); */
  /* print music */
  p = head;
  count = zero_barcount(&foundbar);
  while ((p != NULL) && (count < bars)) {
    if (p->type == field) {
      setline(fresh);
    };
    printf("%s", p->text);
    if (p->type == field) {
      setline(postfield);
      setline(fresh);
    } else {
      setline(midmusic);
    };
    count = new_barcount(p->type, &foundbar, count);
    if ((count == bars) && (p->type == barline)) {
      setline(endmusicline);
    };
    p = p->next;
  };
  if (linestat == midmusic) {
    setline(termination);
  };
  if (bars > 0) {
    /* print out any w: lines */
    donewords = 0;
    wordline = 0;
    while (donewords == 0) {
      p = head;
      foundtext = 0;
      count = zero_barcount(&foundbar);
      while ((p != NULL) && (count < bars)) {
        barlyrics = p->lyrics;
        for (i=0; i<wordline; i++) {
          if (barlyrics != NULL) {
            barlyrics = barlyrics->nextverse;
          };
        };
        if (barlyrics != NULL) {
          if (foundtext == 0) {
            setline(fresh);
            printf("w:");
            foundtext = 1;
          };
          printf(barlyrics->words);
        };
        count = new_barcount(p->type, &foundbar, count);
        p = p->next;
      };
      if (foundtext == 0) {
        donewords = 1;
      } else {
        setline(postfield);
        setline(fresh);
      };
      wordline = wordline + 1;
    };
  };
  /* move head on and free up space used by stuff printed out */
  count = zero_barcount(&foundbar);
  p = head;
  foundbar = 0;
  while ((p != NULL) && (count < bars)) {
    if (p != NULL) {
      free(p->text);
      q = p->lyrics;
      while (q != NULL) {
        free(q->words);
        r = q->nextverse;
        free(q);
        q = r;
      };
      count = new_barcount(p->type, &foundbar, count);
      nextp = p->next;
      free(p);
      p = nextp;
    };
    head = p;
  };
  if (head == NULL) {
    tail = NULL;
  };
  return(count);
}

void complete_bars(v)
/* mark all bars as completed (i.e. having associated w: fields parsed) */
/* and out put all music lines which contain the full set of bars */
/* part of new linebreak option (-n) */
struct voicetype *v;
{
  int bars_done;

  v->bars_complete = v->bars_complete + v->barcount;
  v->barcount = 0;
  while (v->bars_complete > v->bars_remaining) {
    bars_done = flush_abctext(v->bars_remaining, endmusicline);
    setline(fresh);
    v->bars_complete = v->bars_complete - bars_done;
    v->bars_remaining = v->bars_remaining - bars_done;
    if (v->bars_remaining == 0) {
      v->bars_remaining = bars_per_line;
    };
  };
}

void complete_all(v, termination)
struct voicetype *v;
enum linestattype termination;
/* output all remaining music and fields */
/* part of new linebreak option (-n) */
{
  int bars_done;

  complete_bars(v);
  bars_done = flush_abctext(v->bars_remaining+1, termination);
  v->bars_complete = v->bars_complete - bars_done;
  v->bars_remaining = v->bars_remaining - bars_done;
  if (v->bars_remaining == 0) {
    v->bars_remaining = bars_per_line;
  };
  head = NULL;
  tail = NULL;
  voice[this_voice].currentline = NULL;
}

static struct abctext* newabctext(t)
enum abctype t;
/* called at newlines and barlines */
/* adds current output text to linked list structure */
/* part of new linebreak option (-n) */
{
  struct abctext* p;
  int save_voice;

  if (output_on == 0) {
    p = NULL;
    return(p);
  };
  if (newbreaks) {
/*
    if ((t == field) && (!xinbody || (this_voice != next_voice))) {
*/
    if (t == field) {
      complete_all(&voice[this_voice], midmusic);
      this_voice = next_voice;
    };
    p = (struct abctext*) checkmalloc(sizeof(struct abctext));
    p->text = addstring(tmp);
    tmp[0] = '\0';
    p->next = NULL;
    p->type = t;
    p->lyrics = NULL;
    if (t == bar) {
      p->notes = notecount;
      totalnotes = totalnotes + notecount;
      notecount = 0;
    } else {
      p->notes = 0;
    };
    if (xinbody) {
      voice[this_voice].barcount = new_barcount(t, 
                       &voice[this_voice].foundbar, 
                        voice[this_voice].barcount);
    };
    if (head == NULL) {
      head = p;
      tail = p;
    } else {
      tail->next = p;
      tail = p;
    };
    if ((t != field) && (voice[this_voice].currentline == NULL)) {
      voice[this_voice].currentline = p;
    };
  } else {
    printf("%s", tmp);
    tmp[0] = '\0';
    p = NULL;
  };
  inmusic = 1;
  return(p);
}

static int nextnotes()
/* return the number of notes in the next bar */
/* part of new linebreak option (-n) */
{
  int n, got;
  struct abctext* p;

  p = head;
  n = 100;
  got = 0;
  while ((p != NULL) && (!got)) {
    if (p->type == bar) {
      n = p->notes;
      got = 1;
    } else {
      p = p->next;
    };
  };
  return(n);
}

static void reduce(a, b)
int *a, *b;
{
  int t, n, m;

  /* find HCF using Euclid's algorithm */
  if (*a > *b) {
    n = *a;
    m = *b;
  } else {
    n = *b;
    m = *a;
  };
  while (m != 0) {
    t = n % m;
    n = m;
    m = t;
  };
  *a = *a/n;
  *b = *b/n;
}

static void addunits(n, m)
int n, m;
/* add fraction n/m to count */
{
  count.num = n*count.denom + count.num*(m*unitlen.denom);
  count.denom = (m*unitlen.denom)*count.denom;
  reduce(&count.num, &count.denom);
}

void event_init(argc, argv, filename)
int argc;
char* argv[];
char** filename;
/* routine called on program start-up */
{
  int targ, narg;

  if ((getarg("-h", argc, argv) != -1) || (argc < 2)) {
    printf("abc2abc version 1.19\n");
    printf("Usage: abc2abc <filename> [-s] [-n X] [-b] [-r] [-e] [-t X]\n");
    printf("       [-u] [-d] [-v] [-V X] [-X n]\n");
    printf("  -s for new spacing\n");
    printf("  -n X to re-format the abc with a new linebreak every X bars\n");
    printf("  -b to remove bar checking\n");
    printf("  -r to remove repeat checking\n");
    printf("  -e to remove all error reports\n");
    printf("  -t X to transpose X semitones\n");
    printf("  -u to update notation ([] for chords and () for slurs)\n");
    printf("  -d to notate with doubled note lengths\n");
    printf("  -v to notate with halved note lengths\n");
    printf("  -V X to output only voice X\n");
    printf("  -X n renumber the all X: fields as n, n+1, ..\n");
    exit(0);
  } else {
    *filename = argv[1];
  };
  if (getarg("-u", argc, argv) == -1) {
    cleanup = 0;
  } else {
    cleanup = 1;
  };
  if (getarg("-s", argc, argv) == -1) {
    newspacing = 0;
  } else {
    newspacing = 1;
  };
  narg = getarg("-X", argc, argv);
  if (narg == -1) {
    newrefnos = 0;
  } else {
    newrefnos = 1;
    if (narg < argc) {
      newref = readnumf(argv[narg]);
    } else {
      newref = 1;
    };
  };
  if (getarg("-e", argc, argv) == -1) {
    echeck = 1;
  } else {
    echeck = 0;
  };
  narg = getarg("-n", argc, argv);
  if (narg == -1) {
    newbreaks = 0;
  } else {
    newbreaks = 1;
    if (narg >= argc) {
      event_error("No value for bars per line after -n");
      bars_per_line = 4;
    } else {
      bars_per_line = readnumf(argv[narg]);
      if (bars_per_line < 1) {
        bars_per_line = 4;
      };
    };
  };
  if (getarg("-b", argc, argv) != -1) {
    barcheck = 0;
  } else {
    barcheck = 1;
  };
  if (getarg("-r", argc, argv) != -1) {
    repcheck = 0;
  } else {
    repcheck = 1;
  };
  if (getarg("-v", argc, argv) != -1) {
    lenfactor.num = 1;
    lenfactor.denom = 2;
  } else {
    if (getarg("-d", argc, argv) != -1) {
      lenfactor.num = 2;
      lenfactor.denom = 1;
    } else {
      lenfactor.num = 1;
      lenfactor.denom = 1;
    };
  };
  targ = getarg("-t", argc, argv);
  if (targ == -1) {
    transpose = 0;
  } else {
    if (targ >= argc) {
      event_error("No tranpose value supplied");
    } else {
      if (*argv[targ] == '-') {
        transpose = -readnumf(argv[targ]+1);
      } else {
        transpose = readnumf(argv[targ]);
      };
    };
  };
  targ = getarg("-V", argc, argv);
  if (targ != -1) {
    selected_voice  = readnumf(argv[targ]);
  };

  /* printf("%% output from abc2abc\n"); */
  startline = 1;
  blankline = 0;
  xinbody =0;
  inmusic = 0;
  inchord = 0;
  ingrace = 0;
  head = NULL;
  tail = NULL;
  tmp[0] = '\0';
  totalnotes = 0;
}

void emit_string(s)
char *s;
/* output string */
{
  if (output_on) {
    strcpy(tmp+strlen(tmp), s);
  };
}

void emit_char(ch)
char ch;
/* output single character */
{
  char *place;

  if (output_on) {
    place = tmp+strlen(tmp);
    *place = ch;
    *(place+1) = '\0';
  };
}

void emit_int(n)
int n;
/* output integer */
{
  if (output_on) {
    sprintf(tmp+strlen(tmp), "%d", n);
  };
}

void emit_string_sprintf(s1, s2)
char *s1;
char *s2;
/* output string containing string expression %s */
{
  if (output_on) {
    sprintf(tmp+strlen(tmp), s1, s2);
  };
}

void emit_int_sprintf(s, n)
char *s;
int n;
/* output string containing int expression %d */
{
  if (output_on) {
    sprintf(tmp+strlen(tmp), s, n);
  };
}

void unemit_inline()
/* remove previously output start of inline field */
/* needed for -V voice selection option           */
{
  int len;

  len = strlen(tmp);
  if ((len > 0) && (tmp[len-1] == '[')) {
    tmp[len-1] = '\0'; /* delete last character */
  } else {
    event_error("Internal error - Could not delete [");
  };
}

static void close_newabc()
/* output all remaining abc_text elements */
/* part of new linebreak option (-n) */
{
  if (newbreaks) {
    complete_all(&voice[this_voice], endmusicline);
    if (linestat == midmusic) setline(endmusicline);
    setline(fresh);
  };
}

void event_eof()
{
  close_newabc();
}

void event_blankline()
{
  output_on = 1;
  close_newabc();
  if (newbreaks) printf("\n");
  xinbody = 0;
  xinhead = 0;
  parseroff();
  blankline = 1;
}

void event_text(p)
char *p;
{
  emit_string_sprintf("%%%s", p);
  inmusic = 0;
}

void event_reserved(p)
char p;
{
  emit_char(p);
  inmusic = 0;
}

void event_tex(s)
char *s;
{
  emit_string(s);
  inmusic = 0;
}

void event_linebreak()
{
  if (newbreaks) {
    if (!purgespace(tmp)) {
      if (inmusic) {
        newabctext(bar);
      } else {
        newabctext(field);
      };
    };
  } else {
    newabctext(bar);
    if (output_on) {
      printf("\n");
    };
    /* don't output new line if voice is already suppressed
       otherwise we will get lots of blank lines where we
       are suppressing output. [SS] feb-10-2002.
     */
  };
}

void event_startmusicline()
/* encountered the start of a line of notes */
{
  voice[this_voice].currentline = NULL;
  complete_bars(&voice[this_voice]);
}

void event_endmusicline(endchar)
char endchar;
/* encountered the end of a line of notes */
{
}

void event_error(s)
char *s;
{
  if (echeck) {
   printf("\n%%Error : %s\n", s);
  };
}

void event_warning(s)
char *s;
{
  if (echeck) {
   printf("\n%%Warning : %s\n", s);
  };
}

void event_comment(s)
char *s;
{
  if (newbreaks && (!purgespace(tmp))) {
    if (inmusic) {
      newabctext(bar);
    } else {
      newabctext(field);
    };
  };
  emit_string_sprintf("%%%s", s);
  inmusic = 0;
}

void event_specific(p, s)
char *p, *s;
{
  emit_string("%%");
  emit_string(p);
  emit_string(s);
  inmusic = 0;
}

void event_info(f)
/* handles info field I: */
char *f;
{
  emit_string_sprintf("I:%s", f);
  inmusic = 0;
}


void event_field(k, f)
char k;
char *f;
{
  emit_char(k);
  emit_char(':');
  emit_string(f);
  inmusic = 0;
}

struct abctext* getbar(place)
struct abctext *place;
/* find first element in list which is a bar of music */
{
  struct abctext *newplace;

  newplace = place;
  while ((newplace != NULL) &&
         ((newplace->type != bar) || 
          (newplace->notes == 0))) {
    newplace = newplace->next;
  };
  return(newplace);    
}

struct abctext* getnextbar(place)
struct abctext *place;
/* find next element in list which is a bar of music */
{
  struct abctext *newplace;

  newplace = place;
  if (newplace != NULL) {
    newplace = getbar(newplace->next);
  };
  return(newplace);
};

append_lyrics(place, newwords)
struct abctext *place;
char *newwords;
/* add lyrics to end of lyric list associated with bar */
{
  struct lyricwords* new_words;
  struct lyricwords *new_place;

  if (place == NULL) {
    return;
  };
  /* printf("append_lyrics has %s at %s\n", newwords, place->text); */
  new_words = (struct lyricwords*)checkmalloc(sizeof(struct lyricwords));
  /* add words to bar */
  new_words->nextverse = NULL;
  new_words->words = addstring(newwords);
  if (place->lyrics == NULL) {
    place->lyrics = new_words;
  } else {
    new_place = place->lyrics;
    /* find end of list */
    while (new_place->nextverse != NULL) {
      new_place = new_place->nextverse;
    };
    new_place->nextverse = new_words;
  };
}

struct abctext* apply_bar(syll, place, notesleft, barwords)
/* advance to next bar (on finding '|' in a w: field) */
char* syll;
struct abctext *place;
int *notesleft;
struct vstring *barwords;
{
  struct lyricwords* new_words;
  struct abctext* new_place;

  if (place == NULL) {
    return(NULL);
  };
  new_place = place;
  addtext(syll, barwords);
  append_lyrics(place, barwords->st);
  /* go on to next bar */
  clearvstring(barwords);
  new_place = getnextbar(place);
  if (new_place != NULL) {
    *notesleft = new_place->notes;
  };
  return(new_place); 
}

struct abctext* apply_syllable(syll, place, notesleft, barwords)
/* attach syllable to appropriate place in abctext structure */
char* syll;
struct abctext *place;
int *notesleft;
struct vstring *barwords;
{
  struct lyricwords* new_words;
  struct abctext* new_place;
  char msg[80];

  if (place == NULL) {
    sprintf(msg, "Cannot find note to match \"%s\"", syll);
    event_error(msg);
    return(NULL);
  };
  new_place = place;
  addtext(syll, barwords);
  *notesleft = *notesleft - 1;
  if (*notesleft == 0) {
    append_lyrics(place, barwords->st);
    /* go on to next bar */
    clearvstring(barwords);
    new_place = getnextbar(place);
    if (new_place != NULL) {
      *notesleft = new_place->notes;
    };
  };
  return(new_place); 
}

void parse_words(p)
char* p;
/* Break up a line of lyrics (w: ) into component syllables */
{
  struct vstring syll;
  struct vstring barwords;
  char* q;
  unsigned char ch;
  int errors;
  int found_hyphen;

  struct abctext *place;
  int notesleft;

  if (!xinbody) {
    event_error("w: field outside tune body");
    return;
  };
  place = getbar(voice[this_voice].currentline);
  if (place == NULL) {
    event_error("No music to match w: line to");
    return;
  };
  notesleft = voice[this_voice].currentline->notes;
  initvstring(&barwords);
  errors = 0;
  if (place == NULL) {
    event_error("No notes to match words");
    return;
  };
  initvstring(&syll);
  q = p;
  skipspace(&q);
  while (*q != '\0') {
    found_hyphen = 0;
    clearvstring(&syll);
    ch = *q;
    while(ch=='|') {
      addch('|', &syll);
      addch(' ', &syll);
      place = apply_bar(syll.st, place, &notesleft, &barwords);
      clearvstring(&syll);
      q++;
      ch = *q;
    };
    /* PCC seems to require (ch != ' ') on the next line */
    /* presumably PCC's version of ispunct() thinks ' ' is punctuation */
    while (((ch>127)||isalnum(ch)||ispunct(ch))&&(ch != ' ')&&
           (ch != '_')&&(ch != '-')&&(ch != '*')&& (ch != '|')) {
      if ((ch == '\\') && (*(q+1)=='-')) {
        addch('\\', &syll);
        ch = '-';
        q++;
      };
      /* syllable[i] = ch; */
      addch(ch, &syll);
      q++;
      ch = *q;
    };
    skipspace(&q);
    if (ch == '-') {
      found_hyphen = 1;
      addch(ch, &syll);
      while (isspace(ch)||(ch=='-')) {
        q++;
        ch = *q;
      };
    };
    if (syll.len > 0) {
      if (!found_hyphen) {
        addch(' ', &syll);
      };
      place = apply_syllable(syll.st, place, &notesleft, &barwords);
    } else {
      if (ch=='_') {
        clearvstring(&syll);
        addch('_', &syll);
        addch(' ', &syll);
        place = apply_syllable(syll.st, place, &notesleft, &barwords);
        q++;
        ch = *q;
      };
      if (ch=='*') {
        clearvstring(&syll);
        addch('*', &syll);
        addch(' ', &syll);
        place = apply_syllable(syll.st, place, &notesleft, &barwords);
        q++;
        ch = *q;
      };
    }; 
  };
  if (errors > 0) {
    event_error("Lyric line too long for music");
  } else {
    clearvstring(&syll);
  };
  freevstring(&syll);
}

void event_words(p, continuation)
char* p;
int continuation;
/* a w: field has been encountered */
{
  struct vstring afield;

  if (xinbody && newbreaks) {
    parse_words(p);
  } else {
    initvstring(&afield);
    addtext(p, &afield);
    if (continuation) {
      addch(' ', &afield);
      addch('\\', &afield);
    };
    event_field('w', afield.st);
  };
}

void event_part(s)
char* s;
{
  if (xinbody) {
    complete_bars(&voice[this_voice]);
  };
  emit_string_sprintf("P:%s", s);
  inmusic = 0;
}

int setvoice(num)
int num;
/* we need to keep track of current voice for new linebreak handling (-n) */
/* change voice to num. If voice does not exist, start new one */
{
  int i, voice_index;

  i = 0;
  while ((i < voicecount) && (voice[i].number != num)) {
    i = i + 1;
  };
  if ((i < voicecount) && (voice[i].number == num)) {
    voice_index = i;
  } else {
    voice_index = voicecount;
    if (voicecount < MAX_VOICES) {
      voicecount = voicecount + 1;
    } else {
      event_error("Number of voices exceeds static limit MAX_VOICES");
    };
    voice[voice_index].number = num;
    voice[voice_index].barcount = zero_barcount(&voice[voice_index].foundbar);
    voice[voice_index].bars_complete = 0;
    voice[voice_index].bars_remaining = bars_per_line;
  };
  voice[voice_index].currentline = NULL;
  return(voice_index);
}

void event_voice(n, s)
int n;
char *s;
{
  if (xinbody) {
    next_voice = setvoice(n);
  };
  if ((selected_voice != -1) && (n != selected_voice)) {
    if ((inlinefield) && (output_on == 1)) { 
      unemit_inline();
    }; 
    output_on = 0;
  } else { 
    if (output_on == 0) { 
      output_on = 1; 
      if (inlinefield) { 
        emit_string("["); /* regenerate missing [ */
      }; 
    }; 
  }; 
  if (strlen(s) == 0) {
    emit_int_sprintf("V:%d", n);
  } else {
    emit_int_sprintf("V:%d ", n);
    emit_string(s);
  };
  inmusic = 0;
}

void event_length(n)
int n;
{
  struct fract newunit;

  newunit.num = lenfactor.denom;
  newunit.denom = lenfactor.num * n;
  reduce(&newunit.num, &newunit.denom);
  emit_int_sprintf("L:%d/", newunit.num);
  emit_int(newunit.denom);
  unitlen.num = 1;
  unitlen.denom = n;
  inmusic = 0;
}

void event_refno(n)
int n;
{
  if (xinbody) {
    close_newabc();
  };
  output_on = 1;
  if (newrefnos) {
    emit_int_sprintf("X: %d", newref);
    newref = newref + 1;
  } else {
    emit_int_sprintf("X: %d", n);
  };
  parseron();
  xinhead = 1;
  notecount = 0;
  unitlen.num = 0;
  unitlen.denom = 1;
  barlen.num = 0;
  barlen.denom = 1;
  inmusic = 0;
  barcount = 0;
}

void event_tempo(n, a, b, relative, pre, post)
int n, a, b;
int relative;
char *pre;
char *post;
{
  struct fract newlen;

  emit_string("Q:");
  if (pre != NULL) {
    emit_string_sprintf("\"%s\"", pre);
  };
  if (n != 0) {
    if ((a == 0) && (b == 0)) {
      emit_int(n);
    } else {
      if (relative) {
        newlen.num = a * lenfactor.num;
        newlen.denom = b * lenfactor.denom;
        reduce(&newlen.num, &newlen.denom);
        emit_int_sprintf("C%d/", newlen.num);
        emit_int(newlen.denom);
        emit_int_sprintf("=%d", n);
      } else {
        emit_int_sprintf("%d/", a);
        emit_int(b);
        emit_int_sprintf("=%d", n);
      };
    };
  };
  if (post != NULL) {
    emit_string_sprintf("\"%s\"", post);
  };
  inmusic = 0;
}

void event_timesig(n, m, checkbars)
int n, m, checkbars;
{
  if (checkbars == 1) {
    emit_int_sprintf("M:%d/", n);
    emit_int(m);
  } else {
    emit_string("M:none");
    barcheck = 0;
  };
  barlen.num = n;
  barlen.denom = m;
  breakpoint.num = n;
  breakpoint.denom = m;
  if ((n == 9) || (n == 6)) {
    breakpoint.num = 3;
    breakpoint.denom = barlen.denom;
  };
  if (n%2 == 0) {
    breakpoint.num = barlen.num/2;
    breakpoint.denom = barlen.denom;
  };
  barend = n/breakpoint.num;
  inmusic = 0;
}

static void setmap(sf, map)
int sf;
int map[7];
{
  int j;

  for (j=0; j<7; j++) {
    map[j] = 0;
  };
  if (sf >= 1) map['f'-'a'] = 1;
  if (sf >= 2) map['c'-'a'] = 1;
  if (sf >= 3) map['g'-'a'] = 1;
  if (sf >= 4) map['d'-'a'] = 1;
  if (sf >= 5) map['a'-'a'] = 1;
  if (sf >= 6) map['e'-'a'] = 1;
  if (sf >= 7) map['b'-'a'] = 1;
  if (sf <= -1) map['b'-'a'] = -1;
  if (sf <= -2) map['e'-'a'] = -1;
  if (sf <= -3) map['a'-'a'] = -1;
  if (sf <= -4) map['d'-'a'] = -1;
  if (sf <= -5) map['g'-'a'] = -1;
  if (sf <= -6) map['c'-'a'] = -1;
  if (sf <= -7) map['f'-'a'] = -1;
}

static void start_tune()
{
  parseron();
  count.num =0;
  count.denom = 1;
  barno = 0;
  tuplenotes = 0;
  expect_repeat = 0;
  inlinefield = 0;
  if (barlen.num == 0) {
    /* generate missing time signature */
    event_timesig(4, 4, 1);
    inmusic = 0;
    event_linebreak();
  };
  if (unitlen.num == 0) {
    if ((float) barlen.num / (float) barlen.denom < 0.75) {
      unitlen.num = 1;
      unitlen.denom = 16;
    } else {
      unitlen.num = 1;
      unitlen.denom = 8;
    };
  };
  voicecount = 0;
  this_voice = setvoice(1);
  next_voice = this_voice;
}


void event_key(sharps, s, minor, modmap, modmul, gotkey, gotclef, clefname,
          octave, xtranspose, gotoctave, gottranspose)
int sharps;
char *s;
int minor;
char modmap[7];
int modmul[7];
int gotkey, gotclef;
char* clefname;
int octave, xtranspose, gotoctave, gottranspose;
{
  static char* keys[12] = {"Db", "Ab", "Eb", "Bb", "F", "C", 
                           "G", "D", "A", "E", "B", "F#"};

  if (gotkey) {
    setmap(sharps, oldtable);
    newkey = (sharps+7*transpose)%12;
    lines = (sharps+7*transpose)/12;
    if (newkey > 6) {
      newkey = newkey - 12;
      lines = lines + 1;
    };
    if (newkey < -5) {
      newkey = newkey + 12;
      lines = lines - 1;
    };
    setmap(newkey, newtable);
  };
  emit_string("K:");
  if (transpose == 0) {
    emit_string(s);
  } else {
    if (gotkey) {
      emit_string(keys[newkey+5]);
      if (gotclef) {
        emit_string(" ");
      };
    };
    if (gotclef) {
      emit_string_sprintf("clef=%s", clefname);
    };
    if (gotoctave) {
      emit_int_sprintf(" octave=%d", octave);
    };
    if (gottranspose) {
      emit_int_sprintf(" transpose=%d", xtranspose);
    };
  };
  if ((xinhead) && (!xinbody)) {
    xinbody = 1;
    start_tune();
  };
  inmusic = 0;
}

static void printlen(a, b)
int a, b;
{
  if (a != 1) {
    emit_int(a);
  };
  if (b != 1) {
    emit_int_sprintf("/%d", b);
  };
}

void event_rest(n,m)
int n, m;
{
  struct fract newlen;

  inmusic = 1;
  emit_string("z");
  newlen.num = n * lenfactor.num;
  newlen.denom = m * lenfactor.denom;
  reduce(&newlen.num, &newlen.denom);
  printlen(newlen.num, newlen.denom);
  if (inchord) {
    chordcount = chordcount + 1;
  };
  if ((!ingrace) && (!inchord || (chordcount == 1))) {
    addunits(n, m);
  };
  if (tuplenotes != 0) {
    event_error("Rest not allowed in tuple");
  };
}

void event_mrest(n,m)
int n, m;
{
  inmusic = 1;
  emit_string("Z");
  printlen(n,m);
  if (inchord) {
    event_error("Multiple bar rest not allowed in chord");
  };
  if (tuplenotes != 0) {
    event_error("Multiple bar rest not allowed in tuple");
  };
}

void event_bar(type, replist)
int type;
char* replist;
{
  char msg[40];

  if (!purgespace(tmp)) {
    if (inmusic) {
      newabctext(bar);
    } else {
      newabctext(field);
    };
  };
  switch(type) {
  case SINGLE_BAR:
    emit_string_sprintf("|%s", replist);
    break;
  case DOUBLE_BAR:
    emit_string("||");
    break;
  case THIN_THICK:
    emit_string("|]");
    break;
  case THICK_THIN:
    emit_string("[|");
    break;
  case BAR_REP:
    emit_string("|:");
    if ((expect_repeat) && (repcheck)) {
      event_error("Expecting repeat, found |:");
    };
    expect_repeat = 1;
    break;
  case REP_BAR:
    emit_string_sprintf(":|%s", replist);
    if ((!expect_repeat) && (repcheck)) {
      event_error("No repeat expected, found :|");
    };
    expect_repeat = 0;
    break;
  case BAR1:
    emit_string("|1");
    if ((!expect_repeat) && (repcheck)) {
      event_error("found |1 in non-repeat section");
    };
    break;
  case REP_BAR2:
    emit_string(":|2");
    if ((!expect_repeat) && (repcheck)) {
      event_error("No repeat expected, found :|2");
    };
    expect_repeat = 0;
    break;
  case DOUBLE_REP:
    emit_string("::");
    if ((!expect_repeat) && (repcheck)) {
      event_error("No repeat expected, found ::");
    };
    expect_repeat = 1;
    break;
  };
  if ((count.num*barlen.denom != barlen.num*count.denom) &&
      (count.num != 0) && (barno != 0) && (barcheck)) {
    sprintf(msg, "Bar %d is %d/%d not %d/%d", barno, 
           count.num, count.denom,
           barlen.num, barlen.denom );
    event_error(msg);
  };
  newabctext(barline);
  barno = barno + 1;
  count.num = 0;
  count.denom = 1;
}

void event_space()
{
  if (!newspacing) {
    emit_string(" ");
  };
}

void event_graceon()
{
  emit_string("{");
  ingrace = 1;
}

void event_graceoff()
{
  emit_string("}");
  ingrace = 0;
}

void event_rep1()
{
  emit_string(" [1");
}

void event_rep2()
{
  emit_string(" [2");
}

void event_playonrep(s)
char*s;
{
  emit_string_sprintf(" [%s", s);
}

void event_broken(type, n)
int type, n;
{
  int i;

  if (type == GT) {
    for (i=0; i<n; i++) {
      emit_char('>');
    };
  } else {
    for (i=0; i<n; i++) {
      emit_char('<');
    };
  };
}

void event_tuple(n, q, r)
int n, q, r;
{
  emit_int_sprintf("(%d", n);
  if (tuplenotes != 0) {
    event_error("tuple within tuple not allowed");
  };
  if (q != 0) {
    emit_int_sprintf(":%d", q);
    tuplefactor.num = q;
    tuplefactor.denom = n;
    if (r != 0) {
      emit_int_sprintf(":%d", r);
      tuplenotes = r;
    } else {
      tuplenotes = n;
    };
  } else {
    tuplenotes = n;
    tuplefactor.denom = n;
    if ((n == 2) || (n == 4) || (n == 8)) tuplefactor.num = 3;
    if ((n == 3) || (n == 6)) tuplefactor.num = 2;
    if ((n == 5) || (n == 7) || (n == 9)) {
      if ((barlen.num % 3) == 0) {
        tuplefactor.num = 3;
      } else {
        tuplefactor.num = 2;
      };
    };
  };
}

void event_startinline()
{
  emit_string("[");
  inlinefield = 1;
}

void event_closeinline()
{
  emit_string("]");
  inmusic = 1;
  inlinefield = 0;
}

void event_chord()
{
  if (cleanup) {
    if (inchord) {
      emit_string("]");
    } else {
      emit_string("[");
    };
  } else {
    emit_string("+");
  };
  inmusic = 1;
  inchord = 1 - inchord;
  chordcount = 0;
}

void event_chordon()
{
  emit_string("[");
  inmusic = 1;
  inchord = 1;
  chordcount = 0;
}

void event_chordoff()
{
  emit_string("]");
  inmusic = 1;
  inchord = 0;
}

static void splitstring(s, sep, handler)
char* s;
char sep;
void (*handler)();
/* this routine splits the string into fields using semi-colon */
/* and calls handler for each sub-string                       */
{
  char* out;
  char* p;
  int fieldcoming;

  p = s;
  fieldcoming = 1;
  while (fieldcoming) {
    out = p;
    while ((*p != '\0') && (*p != sep)) p = p + 1;
    if (*p == sep) {
      *p = '\0';
      p = p + 1;
    } else {
      fieldcoming = 0;
    };
    (*handler)(out);
  };
}

void event_handle_gchord(s)
/* deals with an accompaniment (guitar) chord */
/* either copies it straight out or transposes it */
char* s;
{
  char newchord[50];
  static int offset[7] = {9, 11, 0, 2, 4, 5, 7};
  static char* sharproots[12] = {"C", "C#", "D", "D#", "E", "F",
                            "F#", "G", "G#", "A", "A#", "B"};
  static char* flatroots[12] = {"C", "Db", "D", "Eb", "E", "F",
                            "Gb", "G", "Ab", "A", "Bb", "B"};
  static char* sharpbases[12] = {"c", "c#", "d", "d#", "e", "f",
                            "f#", "g", "g#", "a", "a#", "b"};
  static char* flatbases[12] = {"c", "db", "d", "eb", "e", "f",
                            "gb", "g", "ab", "a", "bb", "b"};
  char** roots;
  char** bases;
  int chordstart;

  if ((transpose == 0) || (*s == '_') || (*s == '^') || (*s == '<') ||
      (*s == '>') || (*s == '@')) {
    emit_string_sprintf("\"%s\"", s);
  } else {
    char* p;
    int pitch;
    int j;

    if (newkey >= 0) {
      roots = sharproots;
      bases = sharpbases;
    } else {
      roots = flatroots;
      bases = flatbases;
    };
    p = s;
    chordstart = 1;
    j = 0;
    while (*p != '\0') {
      if (chordstart) {
        if ((*p >= 'A') && (*p <= 'G')) {
          pitch = (offset[(int) *p - ((int) 'A')] + transpose)%12;
          p = p + 1;
          if (*p == 'b') {
            pitch = pitch - 1;
            p = p + 1;
          };
          if (*p == '#') {
            pitch = pitch + 1;
            p = p + 1;
          };
          pitch = (pitch + 12)%12;
          strcpy(&newchord[j], roots[pitch]);
          j = strlen(newchord);
          chordstart = 0;
        } else {
          if ((*p >= 'a') && (*p <= 'g')) {
            pitch = (offset[(int) *p - ((int) 'a')] + transpose)%12;
            p = p + 1;
            if (*p == 'b') {
              pitch = pitch - 1;
              p = p + 1;
            };
            if (*p == '#') {
              pitch = pitch + 1;
              p = p + 1;
            };
            pitch = (pitch + 12)%12;
            strcpy(&newchord[j], bases[pitch]);
            j = strlen(newchord);
            chordstart = 0;
          } else {
            if (isalpha(*p)) {
              chordstart = 0;
            };
            newchord[j] = *p;
            p = p + 1;
            j = j + 1;
            newchord[j] = '\0';
          };
        };
      } else {
        if ((*p == '/') || (*p == '(') || (*p == ' ')) {
          chordstart = 1;
        };
        newchord[j] = *p;
        p = p + 1;
        j = j + 1;
        newchord[j] = '\0';
      };
      if (j >= 49) {
        event_error("guitar chord contains too much text");
        while (*p != '\0') {
          p = p + 1;
        };
      };
    };
    emit_string_sprintf("\"%s\"", newchord);
  };
}

void event_gchord(s)
char* s;
{
  splitstring(s, ';', event_handle_gchord);
}

void event_instruction(s)
char* s;
{
  emit_string_sprintf("!%s!", s);
}

void event_slur(t)
int t;
{
  if (cleanup) {
    if (t) {
      emit_string("(");
    } else {
      emit_string(")");
    };
  } else {
    emit_string("s");
  };
}

void event_sluron(t)
int t;
{
  emit_string("(");
}

void event_sluroff(t)
int t;
{
  emit_string(")");
}

void event_tie()
{
  emit_string("-");
}

void event_lineend(ch, n)
char ch;
int n;
{
  int i;

  if (!newbreaks) {
    for (i = 0; i<n; i++) {
      emit_char(ch);
    };
  };
}

void event_note(decorators, xaccidental, xmult, xnote, xoctave, n, m)
int decorators[DECSIZE];
int xmult;
char xaccidental, xnote;
int xoctave, n, m;
{
  int t;
  struct fract barpoint;
  struct fract newlen;
  int mult;
  char accidental, note;
  int octave;

  if (transpose == 0) {
    accidental = xaccidental;
    mult = xmult;
    note = xnote;
    octave = xoctave;
  } else {
    int val, newval;
    int acc;
    char *anoctave = "cdefgab";

    octave = xoctave;
    val = (int) ((long) strchr(anoctave, xnote) - (long) anoctave);
    newval = val + lines;
    octave = octave + (newval/7);
    newval = newval % 7;
    if (newval < 0) {
      newval = newval + 7;
      octave = octave - 1;
    };
    note = *(anoctave+newval);
    if (xaccidental == ' ') {
      accidental = ' ';
    } else {
      switch (xaccidental) {
      case '_':
        acc = -xmult;
        break;
      case '^':
        acc = xmult;
        break;
      case '=':
        acc = 0;
        break;
      default:
        event_error("Internal error");
      };
      acc = acc - oldtable[(int)anoctave[val] - (int)'a'] + 
                  newtable[(int)anoctave[newval] - (int)'a'];
      mult = 1;
      accidental = '=';
      if (acc > 0) {
        accidental = '^';
        mult = acc;
      };
      if (acc < 0) {
        accidental = '_';
        mult = -acc;
      };
    };
  };    
  if (!ingrace) {
    notecount = notecount + 1;
  };
  for (t=0; t<DECSIZE; t++) {
    if (decorators[t]) {
      emit_char(decorations[t]);
    };
  };
  if (mult == 2) {
    emit_char(accidental);
  };
  if (accidental != ' ') {
    emit_char(accidental);
  };
  if (octave >= 1) {
    emit_char(note);
    t = octave;
    while (t > 1) {
      emit_string("'");
      t = t - 1;
    };
  } else {
    emit_char((char) ((int)note + 'C' - 'c'));
    t = octave;
    while (t < 0) {
      emit_string(",");
      t = t + 1;
    };
  };
  newlen.num = n * lenfactor.num;
  newlen.denom = m * lenfactor.denom;
  reduce(&newlen.num, &newlen.denom);
  printlen(newlen.num, newlen.denom);
  if (inchord) {
    chordcount = chordcount + 1;
  };
  if ((!ingrace) && (!inchord || (chordcount == 1))) {
    if (tuplenotes == 0) {
      addunits(n, m);
    } else {
      addunits(n*tuplefactor.num, m*tuplefactor.denom);
      tuplenotes = tuplenotes - 1;
    };
  };
  if (newspacing) {
    barpoint.num = count.num * breakpoint.denom;
    barpoint.denom = breakpoint.num * count.denom;
    reduce(&barpoint.num, &barpoint.denom);
    if ((barpoint.denom == 1) && (barpoint.num != 0) && 
        (barpoint.num != barend)) {
      emit_string(" ");
    };
  };
}

void event_abbreviation(char symbol, char *string, char container)
/* a U: field has been found in the abc */
{
  if (container == '!') {
    emit_string("U:");
    emit_char(symbol);
    emit_string_sprintf(" = !%s!", string);
  } else {
    emit_string("U:");
    emit_char(symbol);
    emit_string_sprintf(" = %s", string);
  };
  inmusic = 0;
}
