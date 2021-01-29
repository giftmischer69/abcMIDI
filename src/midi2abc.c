/*
 * midi2abc - program to convert MIDI files to abc notation.
 * Copyright (C) 1998 James Allwright
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

/* new midi2abc - converts MIDI file to abc format files
 * 
 *
 * re-written to use dynamic data structures 
 *              James Allwright
 *               5th June 1998
 *
 * added output file option -o
 * added summary option -sum
 *                Seymour Shlien  30/1/00
 *
 * based on public domain 'midifilelib' package.
 *
 */


#include <stdio.h>
#ifdef PCCFIX
#define stdout 1
#endif

/* define USE_INDEX if your C libraries have index() instead of strchr() */
#ifdef USE_INDEX
#define strchr index
#endif

#ifdef ANSILIBS
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#else
extern char* malloc();
extern char* strchr();
#endif
#include "midifile.h"
#define BUFFSIZE 200
/* declare MIDDLE C */
#define MIDDLE 72

void initfuncs();
static FILE *F;
static FILE *outhandle; /* for producing the abc file */
int division;    /* from the file header */
long tempo = 500000; /* the default tempo is 120 beats/minute */
int unitlen;
long laston = 0;
char textbuff[BUFFSIZE];
int trans[256], back[256];
char atog[256];
int symbol[256];
int key[12];
int sharps;
int xchannel;
int trackno, maintrack;
int format;
int xunit;
int extractm, extractl, extracta, guessa, summary; /* command-line options */
int keep_short; /* -s option : do not discard very short notes */
int swallow_rests; /* corresponds to -sr option */
int no_triplets; /* -nt option - do not look for trplets or broken rhythm */
int asig, bsig;
int Qval;
int karaoke, inkaraoke;
int midline;

struct anote {
  int pitch;
  int chan;
  int vel;
  long time;
  long dtnext;
  long tplay;
  int xnum;
  int playnum;
  int denom;
};

/* linked list of notes */
struct listx {
  struct listx* next;
  struct anote* note;
};

/* linked list of text items (strings) */
struct tlistx {
  struct tlistx* next;
  char* text;
  long when;
};

/* a MIDI track */
struct atrack {
  struct listx* head;
  struct listx* tail;
  struct tlistx* texthead;
  struct tlistx* texttail;
  int notes;
  long tracklen;
  long startwait;
  int startunits;
  int drumtrack;
};

/* can cope with up to 64 track MIDI files */
struct atrack track[64];
int trackcount = 0;

/* double linked list of notes */
/* used for temporary list of chords while abc is being generated */
struct dlistx {
  struct dlistx* next;
  struct dlistx* last;
  struct anote* note;
};
/* head and tail of list of notes still playing */
/* used while MIDI file is being parsed */
struct dlistx* playinghead;
struct dlistx* playingtail; 
/* head and tail of list of notes in current chord playing */
/* used while abc is being generated */
struct dlistx* chordhead;
struct dlistx* chordtail;


void fatal_error(s)
char* s;
/* fatal error encounterd - abort program */
{
  fprintf(stderr, "%s\n", s);
  exit(1);
}

void event_error(s)
char *s;
/* problem encountered but OK to continue */
{
  char msg[160];

  sprintf(msg, "Error: Time=%ld Track=%d %s\n", Mf_currtime, trackno, s);
  printf(msg);
}

int* checkmalloc(bytes)
/* malloc with error checking */
int bytes;
{
  int *p;

  p = (int*) malloc(bytes);
  if (p == NULL) {
    fatal_error("Out of memory error - cannot malloc!");
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

void scannotes(trackno)
int trackno;
/* diagnostic routine to output notes in a track */
{
  struct listx* i;

  i = track[trackno].head;
  while (i != NULL) {
    printf("Pitch %d chan %d vel %d time %ld xnum %d playnum %d\n",
            i->note->pitch, i->note->chan, 
            i->note->vel, i->note->dtnext,
            i->note->xnum, i->note->playnum);
    i = i->next;
  };
}

void printchordlist()
/* diagnostic routine */
{
  struct dlistx* i;

  i = chordhead;
  printf("----CHORD LIST------\n");
  while(i != NULL) {
    printf("pitch %d len %d\n", i->note->pitch, i->note->playnum);
    if (i->next == i) {
      fatal_error("Loopback problem!");
    };
    i = i->next;
  };
}

void checkchordlist()
/* diagnostic routine */
/* validates data structure */
{
  struct dlistx* i;
  int n;

  if ((chordhead == NULL) && (chordtail == NULL)) {
    return;
  };
  if ((chordhead == NULL) && (chordtail != NULL)) {
    fatal_error("chordhead == NULL and chordtail != NULL");
  };
  if ((chordhead != NULL) && (chordtail == NULL)) {
    fatal_error("chordhead != NULL and chordtail == NULL");
  };
  if (chordhead->last != NULL) {
    fatal_error("chordhead->last != NULL");
  };
  if (chordtail->next != NULL) {
    fatal_error("chordtail->next != NULL");
  };
  i = chordhead;
  n = 0;
  while((i != NULL) && (i->next != NULL)) {
    if (i->next->last != i) {
      char msg[80];

      sprintf(msg, "chordlist item %d : i->next->last!", n);
      fatal_error(msg);
    };
    i = i->next;
    n = n + 1;
  };
  /* checkchordlist(); */
}

void addtochord(p)
/* used when printing out abc */
struct anote* p;
{
  struct dlistx* newx;
  struct dlistx* place;

  newx = (struct dlistx*) checkmalloc(sizeof(struct dlistx));
  newx->note = p;
  newx->next = NULL;
  newx->last = NULL;

  if (chordhead == NULL) {
    chordhead = newx;
    chordtail = newx;
    checkchordlist();
    return;
  };
  place = chordhead;
  while ((place != NULL) && (place->note->pitch > p->pitch)) {
    place = place->next;
  };
  if (place == chordhead) {
    newx->next = chordhead;
    chordhead->last = newx;
    chordhead = newx;
    checkchordlist();
    return;
  };
  if (place == NULL) {
    newx->last = chordtail;
    chordtail->next = newx;
    chordtail = newx;
    checkchordlist();
    return;
  };
  newx->next = place;
  newx->last = place->last;
  place->last = newx;
  newx->last->next = newx;
  checkchordlist();
}

struct dlistx* removefromchord(i)
/* used when printing out abc */
struct dlistx* i;
{
  struct dlistx* newi;

  /* remove note from list */
  if (i->last == NULL) {
    chordhead = i->next;
  } else {
    (i->last)->next = i->next;
  };
  if (i->next == NULL) {
    chordtail = i->last;
  } else {
    (i->next)->last = i->last;
  };
  newi = i->next;
  free(i);
  checkchordlist();
  return(newi);
}

int findshortest(gap)
/* find the first note in the chord to terminate */
int gap;
{
  int min, v;
  struct dlistx* p;

  p = chordhead;
  min = gap;
  while (p != NULL) {
    v = p->note->playnum;
    if (v < min) {
      min = v;
    };
    p = p->next;
  };
  return(min);
}

int findana(maintrack, barsize)
/* work out anacrusis from MIDI */
/* look for a strong beat marking the start of a bar */
int maintrack;
int barsize;
{
  int min, mincount;
  int place;
  struct listx* p;

  min = 0;
  mincount = 0;
  place = 0;
  p = track[maintrack].head;
  while ((p != NULL) && (place < barsize)) {
    if ((p->note->vel > min) && (place > 0)) {
      min = p->note->vel;
      mincount = place;
    };
    place = place + (p->note->xnum);
    p = p->next;
  };
  return(mincount);
}

void advancechord(len)
/* adjust note lengths for all notes in the chord */
int len;
{
  struct dlistx* p;

  p = chordhead;
  while (p != NULL) {
    if (p->note->playnum <= len) {
      if (p->note->playnum < len) {
        fatal_error("Error - note too short!");
      };
      /* remove note */
      checkchordlist();
      p = removefromchord(p);
    } else {
      /* shorten note */
      p->note->playnum = p->note->playnum - len;
      p = p->next;
    };
  };
}

void freshline()
/* if the current line of abc or text is non-empty, start a new line */
{
  if (midline == 1) {
    fprintf(outhandle,"\n");
    midline = 0;
  };
}

int testtrack(trackno, barbeats, anacrusis)
/* print out one track as abc */
int trackno, barbeats, anacrusis;
{
  struct listx* i;
  int step, gap;
  int barnotes;
  int barcount;
  int breakcount;

  breakcount = 0;
  chordhead = NULL;
  chordtail = NULL;
  i = track[trackno].head;
  gap = 0;
  if (anacrusis > 0) {
    barnotes = anacrusis;
  } else {
    barnotes = barbeats;
  };
  barcount = 0;
  while((i != NULL)||(gap != 0)) {
    if (gap == 0) {
      /* add notes to chord */
      addtochord(i->note);
      gap = i->note->xnum;
      i = i->next;
      advancechord(0); /* get rid of any zero length notes */
    } else {
      step = findshortest(gap);
      if (step > barnotes) {
        step = barnotes;
      };
      if (step == 0) {
        fatal_error("Advancing by 0 in testtrack!");
      };
      advancechord(step);
      gap = gap - step;
      barnotes = barnotes - step;
      if (barnotes == 0) {
        if (chordhead != NULL) {
          breakcount = breakcount + 1;
        };
        barnotes = barbeats;
        barcount = barcount + 1;
        if (barcount == 4) {
          freshline();
          barcount = 0;
        };
      };
    };
  };
  return(breakcount);
}

void printpitch(j)
/* convert numerical value to abc pitch */
struct anote* j;
{
  int p, po;

  p = j->pitch;
  if (p == -1) {
    fprintf(outhandle,"z");
  } else {
    po = p % 12;
    if ((back[trans[p]] != p) || (key[po] == 1)) {
      fprintf(outhandle,"%c%c", symbol[po], atog[p]);
      back[trans[p]] = p;
    } else {
      fprintf(outhandle,"%c", atog[p]);
    };
    while (p >= MIDDLE + 12) {
      fprintf(outhandle,"'");
      p = p - 12;
    };
    while (p < MIDDLE - 12) {
      fprintf(outhandle,",");
      p = p + 12;
    };
  };
}

void printfract(a, b)
/* print fraction */
/* used when printing abc */
int a, b;
{
  int c, d;

  c = a;
  d = b;
  /* print out length */
  if (((c % 2) == 0) && ((d % 2) == 0)) {
    c = c/2;
    d = d/2;
  };
  if (c != 1) {
    fprintf(outhandle,"%d", c);
  };
  if (d != 1) {
    fprintf(outhandle,"/%d", d);
  };
}

void printchord(len)
/* Print out the current chord. Any notes that haven't            */
/* finished at the end of the chord are tied into the next chord. */
int len;
{
  struct dlistx* i;

  i = chordhead;
  if (i == NULL) {
    /* no notes in chord */
    fprintf(outhandle,"z");
    printfract(len, 2);
    midline = 1;
  } else {
    if (i->next == NULL) {
      /* only one note in chord */
      printpitch(i->note);
      printfract(len, 2);
      midline = 1;
      if (len < i->note->playnum) {
        fprintf(outhandle,"-");
      };
    } else {
      fprintf(outhandle,"[");
      while (i != NULL) {
        printpitch(i->note);
        printfract(len, 2);
        if (len < i->note->playnum) {
          fprintf(outhandle,"-");
        };
        i = i->next;
      };
      fprintf(outhandle,"]");
      midline = 1;
    };
  };
}

char dospecial(i, barnotes, featurecount)
/* identify and print out triplets and broken rhythm */
struct listx* i;
int* barnotes;
int* featurecount;
{
  int v1, v2, v3, vt;
  int xa, xb;
  int pnum;
  long total, t1, t2, t3;

  if ((chordhead != NULL) || (i == NULL) || (i->next == NULL) ||
      (asig%3 == 0) || (asig%2 != 0)) {
    return(' ');
  };
  t1 = i->note->dtnext;
  v1 = i->note->xnum;
  pnum = i->note->playnum;
  if ((v1 < pnum) || (v1 > 1 + pnum) || (pnum == 0)) {
    return(' ');
  };
  t2 = i->next->note->dtnext;
  v2 = i->next->note->xnum;
  pnum = i->next->note->playnum;
  if ((v2 < pnum) || (v2 > 1 + pnum) || (pnum == 0) || (v1+v2 > *barnotes)) {
    return(' ');
  };
  /* look for broken rhythm */
  total = t1 + t2;
  if (total == 0L) {
    /* shouldn't happen, but avoids possible divide by zero */
    return(' ');
  };
  if (((v1+v2)%2 == 0) && ((v1+v2)%3 != 0)) {
    vt = (v1+v2)/2;
      if (vt == validnote(vt)) {
      /* do not try to break a note which cannot be legally expressed */
      switch ((int) ((t1*6+(total/2))/total)) {
        case 2:
          *featurecount = 2;
          i->note->xnum  = vt;
          i->note->playnum = vt;
          i->next->note->xnum  = vt;
          i->next->note->playnum = vt;
          return('<');
          break;
        case 4:
          *featurecount = 2;
          i->note->xnum  = vt;
          i->note->playnum = vt;
          i->next->note->xnum  = vt;
          i->next->note->playnum = vt;
          return('>');
          break;
        default:
          break;
      };
    };
  };
  /* look for triplet */
  if (i->next->next != NULL) {
    t3 = i->next->next->note->dtnext;
    v3 = i->next->next->note->xnum;
    pnum = i->next->next->note->playnum;
    if ((v3 < pnum) || (v3 > 1 + pnum) || (pnum == 0) || 
        (v1+v2+v3 > *barnotes)) {
      return(' ');
    };
    if ((v1+v2+v3)%2 != 0) {
      return(' ');
    };
    vt = (v1+v2+v3)/2;
    if ((vt%2 == 1) && (vt > 1)) {
      /* don't want strange fractions in triplet */
      return(' ');
    };
    total = t1+t2+t3;
    xa = (int) ((t1*6+(total/2))/total); 
    xb = (int) (((t1+t2)*6+(total/2))/total);
    if ((xa == 2) && (xb == 4) && (vt%3 != 0) ) {
      *featurecount = 3;
      *barnotes = *barnotes + vt;
      i->note->xnum = vt;
      i->note->playnum = vt;
      i->next->note->xnum = vt;
      i->next->note->playnum = vt;
      i->next->next->note->xnum = vt;
      i->next->next->note->playnum = vt;
    };
  };
  return(' ');
}

int validnote(n)
int n;
/* work out a step which can be expressed as a musical time */
{
  int v;

  if (n <= 4) {
    v = n;
  } else {
    v = 4;
    while (v*2 <= n) {
      v = v*2;
    };
    if (v + v/2 <= n) {
      v = v + v/2;
    };
  };
  return(v);
}

void handletext(t, textplace)
/* print out text occuring in the body of the track */
/* The text is printed out at the appropriate place within the track */
long t;
struct tlistx** textplace;
{
  char* str;
  char ch;

  while (((*textplace) != NULL) && ((*textplace)->when <= t)) {
    str = (*textplace)->text;
    ch = *str;
    if (((int)ch == '\\') || ((int)ch == '/')) {
      inkaraoke = 1;
    };
    if ((inkaraoke == 1) && (karaoke == 1)) {
      switch(ch) {
        case ' ':
          fprintf(outhandle,"%s", str);
          midline = 1;
          break;
        case '\\':
          freshline();
          fprintf(outhandle,"w:%s", str + 1);
          midline = 1;
          break;
        case '/':
          freshline();
          fprintf(outhandle,"w:%s", str + 1);
          midline = 1;
          break;
        default :
          if (midline == 0) {
            fprintf(outhandle,"%%%s", str);
          } else {
            fprintf(outhandle,"-%s", str);
          };
          break;
      };
    } else {
      freshline();
      if (ch != '%') {
        fprintf(outhandle,"%%%s\n", str);
      } else {
        fprintf(outhandle,"%s\n", str);
      };
    };
    *textplace = (*textplace)->next;
  };
}

void printtrack(trackno, barsize, anacrusis)
/* print out one track as abc */
int trackno, barsize, anacrusis;
{
  struct listx* i;
  struct tlistx* textplace;
  int step, gap;
  int barnotes;
  int barcount;
  long now;
  char broken;
  int featurecount;

  midline = 0;
  featurecount = 0;
  inkaraoke = 0;
  now = 0L;
  broken = ' ';
  chordhead = NULL;
  chordtail = NULL;
  i = track[trackno].head;
  textplace = track[trackno].texthead;
  handletext(now, &textplace);
  gap = track[trackno].startunits;
  if (anacrusis > 0) {
    barnotes = anacrusis;
    barcount = -1;
  } else {
    barnotes = barsize;
    barcount = 0;
  };
  while((i != NULL)||(gap != 0)) {
    if (gap == 0) {
      /* do triplet here */
      if (featurecount == 0) {
        if (!no_triplets) {
          broken = dospecial(i, &barnotes, &featurecount);
        };
      };
      /* add notes to chord */
      addtochord(i->note);
      gap = i->note->xnum;
      now = i->note->time;
      i = i->next;
      advancechord(0); /* get rid of any zero length notes */
      handletext(now, &textplace);
    } else {
      step = findshortest(gap);
      if (step > barnotes) {
        step = barnotes;
      };
      step = validnote(step);
      if (step == 0) {
        fatal_error("Advancing by 0 in printtrack!");
      };
      if (featurecount == 3) {
        fprintf(outhandle,"(3");
      };
      printchord(step);
      if ( featurecount > 0) {
        featurecount = featurecount - 1;
      };
      if ((featurecount == 1) && (broken != ' ')) {
        fprintf(outhandle,"%c", broken);
      };
      advancechord(step);
      gap = gap - step;
      barnotes = barnotes - step;
      if (barnotes == 0) {
        fprintf(outhandle,"|");
        barnotes = barsize;
        barcount = barcount + 1;
        if (barcount == 4) {
          freshline();
          barcount = 0;
        };
      } else {
        if (featurecount == 0) {
          /* note grouping algorithm */
          if (barsize % 6 == 0) {
            if (barnotes % 6 == 0) {
              fprintf(outhandle," ");
            };
          } else {
            if ((barsize % 4 == 0) && (barnotes % 4 == 0)) {
              fprintf(outhandle," ");
            };
          };
        };
      };
    };
  };
  /* print out all extra text */
  while (textplace != NULL) {
    handletext(textplace->when, &textplace);
  };
  freshline();
}

void noteplaying(p)
/* MIDI note starts */
/*used when parsing MIDI file */
struct anote* p;
{
  struct dlistx* newx;

  newx = (struct dlistx*) checkmalloc(sizeof(struct dlistx));
  newx->note = p;
  newx->next = NULL;
  newx->last = playingtail;
  if (playinghead == NULL) {
    playinghead = newx;
  };
  if (playingtail == NULL) {
    playingtail = newx;
  } else {
    playingtail->next = newx;
    playingtail = newx;
  };
}

void addnote(p, ch, v)
/* add structure for note */
/* used when parsing MIDI file */
int p, v;
{
  struct listx* newx;
  struct anote* newnote;

  track[trackno].notes = track[trackno].notes + 1;
  newx = (struct listx*) checkmalloc(sizeof(struct listx));
  newnote = (struct anote*) checkmalloc(sizeof(struct anote));
  newx->next = NULL;
  newx->note = newnote;
  if (track[trackno].head == NULL) {
    track[trackno].head = newx;
    track[trackno].tail = newx;
  } else {
    track[trackno].tail->next = newx;
    track[trackno].tail = newx;
  };
  if (ch == 9) {
    track[trackno].drumtrack = 1;
  };
  newnote->pitch = p;
  newnote->chan = ch;
  newnote->vel = v;
  newnote->time = Mf_currtime;
  laston = Mf_currtime;
  newnote->tplay = Mf_currtime;
  noteplaying(newnote);
}

void addtext(s)
/* add structure for text */
/* used when parsing MIDI file */
char* s;
{
  struct tlistx* newx;

  newx = (struct tlistx*) checkmalloc(sizeof(struct tlistx));
  newx->next = NULL;
  newx->text = addstring(s);
  newx->when = Mf_currtime;
  if (track[trackno].texthead == NULL) {
    track[trackno].texthead = newx;
    track[trackno].texttail = newx;
  } else {
    track[trackno].texttail->next = newx;
    track[trackno].texttail = newx;
  };
}
  
void notestop(p, ch)
/* MIDI note stops */
/* used when parsing MIDI file */
int p, ch;
{
  struct dlistx* i;
  int found;
  char msg[80];

  i = playinghead;
  found = 0;
  while ((found == 0) && (i != NULL)) {
    if ((i->note->pitch == p)&&(i->note->chan==ch)) {
      found = 1;
    } else {
      i = i->next;
    };
  };
  if (found == 0) {
    sprintf(msg, "Note terminated when not on - pitch %d", p);
    event_error(msg);
    return;
  };
  /* fill in tplay field */
  i->note->tplay = Mf_currtime - (i->note->tplay);
  /* remove note from list */
  if (i->last == NULL) {
    playinghead = i->next;
  } else {
    (i->last)->next = i->next;
  };
  if (i->next == NULL) {
    playingtail = i->last;
  } else {
    (i->next)->last = i->last;
  };
  free(i);
}

int filegetc()
{
    return(getc(F));
}

int quantize(trackno, xunit)
/* work out how long each note is in musical time units */
int trackno, xunit;
{
  struct listx* j;
  struct anote* this;
  int spare;
  int toterror;

  /* fix to avoid division by zero errors in strange MIDI */
  if (xunit == 0) {
    return(10000);
  };
  track[trackno].startunits = (2*(track[trackno].startwait + (xunit/4)))/xunit;
  spare = 0;
  toterror = 0;
  j = track[trackno].head;
  while (j != NULL) {
    this = j->note;
    this->xnum = (2*(this->dtnext + spare + (xunit/4)))/xunit;
    this->playnum = (2*(this->tplay + (xunit/4)))/xunit;
    if ((this->playnum == 0) && (keep_short)) {
      this->playnum = 1;
    };
    if ((swallow_rests) && (this->xnum - this->playnum < 2)) {
      this->playnum = this->xnum;
    };
    this->denom = 2;
    spare = spare + this->dtnext - (this->xnum*xunit/this->denom);
    if (spare > 0) {
      toterror = toterror + spare;
    } else {
      toterror = toterror - spare;
    };
    /* gradually forget old errors so that if xunit is slightly off,
       errors don't accumulate over several bars */
    spare = (spare * 96)/100;
    j = j->next;
  };
  return(toterror);
}

int guessana(barbeats)
int barbeats;
/* try to guess length of anacrusis */
{
  int score[64];
  int min, minplace;
  int i,j;

  if (barbeats > 64) {
    fatal_error("Bar size exceeds static limit of 64 units!");
  };
  for (j=0; j<barbeats; j++) {
    score[j] = 0;
    for (i=0; i<trackcount; i++) {
      score[j] = score[j] + testtrack(i, barbeats, j);
      /* restore values to num */
      quantize(i, xunit);
    };
  };
  min = score[0];
  minplace = 0;
  for (i=0; i<barbeats; i++) {
    if (score[i] < min) {
      min = score[i];
      minplace = i;
    };
  };
  return(minplace);
}

void printQ()
/* print out tempo for abc */
{
  float Tnote, freq;

  Tnote = mf_ticks2sec((long)((xunit*unitlen)/4), division, tempo);
  freq = 60.0/Tnote;
  fprintf(outhandle,"Q:1/4=%d\n", (int) (freq+0.5));
  if (summary>0) printf("Tempo: %d quarter notes per minute\n",
    (int) (freq + 0.5));
}

void guesslengths(trackno)
/* work out most appropriate value for a unit of musical time */
int trackno;
{
  int i;
  int trial[100];
  float avlen, factor, tryx;
  long min;

  min = track[trackno].tracklen;
  if (track[trackno].notes == 0) {
    return;
  };
  avlen = ((float)(min))/((float)(track[trackno].notes));
  tryx = avlen * 0.75;
  factor = tryx/100;
  for (i=0; i<100; i++) {
    trial[i] = quantize(trackno, (int) tryx);
    if ((long) trial[i] < min) {
      min = (long) trial[i];
      xunit = (int) tryx;
    };
    tryx = tryx + factor;
  };
}

void postprocess(trackno)
/* This routine calculates the time interval before the next note */
/* called after the MIDI file has been read in */
int trackno;
{
  struct listx* i;

  i = track[trackno].head;
  if (i != NULL) {
    track[trackno].startwait = i->note->time;
  } else {
    track[trackno].startwait = 0;
  };
  while (i != NULL) {
    if (i->next != NULL) {
      i->note->dtnext = i->next->note->time - i->note->time;
    } else {
      i->note->dtnext = i->note->tplay;
    };
    i = i->next;
  };
}

int readnum(num) 
/* read a number from a string */
/* used for processing command line */
char *num;
{
  int t;
  char *p;
  int neg;
  
  t = 0;
  neg = 1;
  p = num;
  if (*p == '-') {
    p = p + 1;
    neg = -1;
  };
  while (((int)*p >= '0') && ((int)*p <= '9')) {
    t = t * 10 + (int) *p - '0';
    p = p + 1;
  };
  return neg*t;
}

int readnump(p) 
/* read a number from a string (subtly different) */
/* used for processing command line */
char **p;
{
  int t;
  
  t = 0;
  while (((int)**p >= '0') && ((int)**p <= '9')) {
    t = t * 10 + (int) **p - '0';
    *p = *p + 1;
  };
  return t;
}

void readsig(a, b, sig)
/* read time signature */
/* used for processing command line */
int *a, *b;
char *sig;
{
  char *p;
  int t;

  p = sig;
  if ((int)*p == 'C') {
    *a = 4;
    *b = 4;
    return;
  };
  *a = readnump(&p);
  if ((int)*p != '/') {
    char msg[80];

    sprintf(msg, "Expecting / in time signature found %c!", *p);
    fatal_error(msg);
  };
  p = p + 1;
  *b = readnump(&p);
  if ((*a == 0) || (*b == 0)) {
    char msg[80];

    sprintf(msg, "%d/%d is not a valid time signature!", *a, *b);
    fatal_error(msg);
  };
  t = *b;
  while (t > 1) {
    if (t%2 != 0) {
      fatal_error("Bad key signature, divisor must be a power of 2!");
    } else {
      t = t/2;
    };
  };
}

int findkey(maintrack)
int maintrack;
/* work out what key MIDI file is in */
/* algorithm is simply to minimize the number of accidentals needed. */
{
  int j;
  int max, min, n[12], key_score[12];
  int minkey, minblacks;
  static int keysharps[12] = {0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5};
  struct listx* p;
  int thispitch;
  int lastpitch;
  int totalnotes;

  /* analyse pitches */
  /* find key */
  for (j=0; j<12; j++) {
    n[j] = 0;
  };
  min = track[maintrack].tail->note->pitch;
  max = min;
  totalnotes = 0;
  for (j=0; j<trackcount; j++) {
    totalnotes = totalnotes + track[j].notes;
    p = track[j].head;
    while (p != NULL) {
      thispitch = p->note->pitch;
      if (thispitch > max) {
        max = thispitch;
      } else {
        if (thispitch < min) {
          min = thispitch;
        };
      };
      n[thispitch % 12] = n[thispitch % 12] + 1;
      p = p->next;
    };
  };
  /* count black notes for each key */
  /* assume pitch = 0 is C */
  minkey = 0;
  minblacks = totalnotes;
  for (j=0; j<12; j++) {
    key[j] = 0;
    key_score[j] = n[(j+1)%12] + n[(j+3)%12] + n[(j+6)%12] +
                   n[(j+8)%12] + n[(j+10)%12];
    /* printf("Score for key %d is %d\n", j, key_score[j]); */
    if (key_score[j] < minblacks) {
      minkey = j;
      minblacks = key_score[j];
    };
  };
  /* do conversion to abc pitches */
  /* Code changed to use absolute rather than */
  /* relative choice of pitch for 'c' */
  /* MIDDLE = (min + (max - min)/2 + 6)/12 * 12; */
  /* Do last note analysis */
  lastpitch = track[maintrack].tail->note->pitch;
  if (minkey != (lastpitch%12)) {
    fprintf(outhandle,"%% Last note suggests ");
    switch((lastpitch+12-minkey)%12) {
    case(2):
      fprintf(outhandle,"Dorian ");
      break;
    case(4):
      fprintf(outhandle,"Phrygian ");
      break;
    case(5):
      fprintf(outhandle,"Lydian ");
      break;
    case(7):
      fprintf(outhandle,"Mixolydian ");
      break;
    case(9):
      fprintf(outhandle,"minor ");
      break;
    case(11):
      fprintf(outhandle,"Locrian ");
      break;
    default:
      fprintf(outhandle,"unknown ");
      break;
    };
    fprintf(outhandle,"mode tune\n");
  };
  /* switch to minor mode if it gives same number of accidentals */
  if ((minkey != ((lastpitch+3)%12)) && 
      (key_score[minkey] == key_score[(lastpitch+3)%12])) {
         minkey = (lastpitch+3)%12;
  };
  /* switch to major mode if it gives same number of accidentals */
  if ((minkey != (lastpitch%12)) && 
      (key_score[minkey] == key_score[lastpitch%12])) {
         minkey = lastpitch%12;
  };
  sharps = keysharps[minkey];
  return(sharps);
}

void setupkey(sharps)
int sharps;
/* set up variables related to key signature */
{
  char sharp[13], flat[13], shsymbol[13], flsymbol[13];
  int j, t, issharp;
  int minkey;

  minkey = (sharps+12)%12;
  if (minkey%2 != 0) {
    minkey = (minkey+6)%12;
  };
  strcpy(sharp,    "ccddeffggaab");
  strcpy(shsymbol, "=^=^==^=^=^=");
  if (sharps == 6) {
    sharp[6] = 'e';
    shsymbol[6] = '^';
  };
  strcpy(flat, "cddeefggaabb");
  strcpy(flsymbol, "=_=_==_=_=_=");
  /* Print out key */

  if (sharps >= 0) {
    if (sharps == 6) {
      fprintf(outhandle,"K:F#");
    } else {
      fprintf(outhandle,"K:%c", sharp[minkey] + 'A' - 'a');
    };
    issharp = 1;
  } else {
    if (sharps == -1) {
      fprintf(outhandle,"K:%c", flat[minkey] + 'A' - 'a');
    } else {
      fprintf(outhandle,"K:%cb", flat[minkey] + 'A' - 'a');
    };
    issharp = 0;
  };
  if (sharps >= 0) {
    fprintf(outhandle," %% %d sharps\n", sharps);
  } else {
    fprintf(outhandle," %% %d flats\n", -sharps);
  };
  key[(minkey+1)%12] = 1;
  key[(minkey+3)%12] = 1;
  key[(minkey+6)%12] = 1;
  key[(minkey+8)%12] = 1;
  key[(minkey+10)%12] = 1;
  for (j=0; j<256; j++) {
    t = j%12;
    if (issharp) {
      atog[j] = sharp[t];
      symbol[j] = shsymbol[t];
    } else {
      atog[j] = flat[t];
      symbol[j] = flsymbol[t];
    };
    trans[j] = 7*(j/12)+((int) atog[j] - 'a');
    if (j < MIDDLE) {
      atog[j] = (char) (int) atog[j] + 'A' - 'a';
    };
    if (key[t] == 0) {
      back[trans[j]] = j;
    };
  };
}

int getarg(option, argc, argv)
/* extract arguments from command line */
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

int huntfilename(argc, argv)
/* look for filename argument if -f option is missing */
/* assumes filename does not begin with '-'           */
char *argv[];
int argc;
{
  int j, place;

  place = -1;
  j = 1;
  while ((place == -1) && (j < argc)) {
    if (strncmp("-", argv[j], 1) != 0) {
      place = j;
    } else {
     if (strchr("ambQkco", *(argv[j]+1)) == NULL) {
       j = j + 1;
     } else {
       j = j + 2;
     };
    };
  };
  return(place);
}

int main(argc,argv)
char *argv[];
int argc;
{
  FILE *efopen();
  int j;
  int barsize, anacrusis, bars;
  int keysig;
  int arg;
  int voiceno;

  arg = getarg("-a", argc, argv);
  if ((arg != -1) && (arg < argc)) {
    anacrusis = readnum(argv[arg]);
  } else {
    anacrusis = 0;
  };
  arg = getarg("-m", argc, argv);
  if ((arg != -1) && (arg < argc)) {
    readsig(&asig, &bsig, argv[arg]);
  } else {
    asig = 4;
    bsig = 4;
  };
  arg = getarg("-Q", argc, argv);
  if (arg != -1) {
    Qval = readnum(argv[arg]);
  } else {
    Qval = 0;
  };
  extractm = (getarg("-xm", argc, argv) != -1);
  extractl = (getarg("-xl", argc, argv) != -1);
  extracta = (getarg("-xa", argc, argv) != -1);
  guessa = (getarg("-ga", argc, argv) != -1);
  keep_short = (getarg("-s", argc, argv) != -1);
  summary = getarg("-sum",argc,argv); 
  swallow_rests = (getarg("-sr", argc, argv) != -1);
  if ((asig*4)/bsig >= 3) {
    unitlen =8;
  } else {
    unitlen = 16;
  };
  arg = getarg("-b", argc, argv);
  if ((arg != -1) && (arg < argc)) {
    bars = readnum(argv[arg]);
  } else {
    bars = 0;
  };
  arg = getarg("-c", argc, argv);
  if ((arg != -1) && (arg < argc)) {
    xchannel = readnum(argv[arg]) - 1;
  } else {
    xchannel = -1;
  };
  arg = getarg("-k", argc, argv);
  if ((arg != -1) && (arg < argc)) {
    keysig = readnum(argv[arg]);
    if (keysig<-6) keysig = 12 - ((-keysig)%12);
    if (keysig>6)  keysig = keysig%12;
    if (keysig>6)  keysig = keysig - 12;
  } else {
    keysig = -50;
  };

  arg = getarg("-o",argc,argv);
  if ((arg != -1) && (arg < argc))  {
    outhandle = efopen(argv[arg],"w");  /* open output abc file */
  } else {
    outhandle = stdout;
  };
  arg = getarg("-nt", argc, argv);
  if (arg == -1) {
    no_triplets = 0;
  } else {
    no_triplets = 1;
  };
  arg = getarg("-f", argc, argv);
  if (arg == -1) {
    arg = huntfilename(argc, argv);
  };
  if ((arg != -1) && (arg < argc)) {
    F = efopen(argv[arg],"rb");
    fprintf(outhandle,"%% input file %s\n", argv[arg]);  
  } else {
    printf("midi2abc version 2.4\n  usage :\n");
    printf("midi2abc <options>\n");
    printf("         -a <beats in anacrusis>\n");
    printf("         -xa  extract anacrusis from file ");
    printf("(find first strong note)\n");
    printf("         -ga  guess anacrusis (minimize ties across bars)\n");
    printf("         -m <time signature>\n");
    printf("         -xm  extract time signature from file\n");
    printf("         -xl  extract absolute note lengths from file\n");
    printf("         -b <bars wanted in output>\n");
    printf("         -Q <tempo in quarter-notes per minute>\n");
    printf("         -k <key signature> -6 to 6 sharps\n");
    printf("         -c <channel>\n");
    printf("         [-f] <input file>\n");
    printf("         -o <output file>\n");
    printf("         -s do not discard very short notes\n");
    printf("         -sr do not notate a short rest after a note\n");
    printf("         -sum summary\n");
    printf("         -nt do not look for triplets or broken rhythm\n");
    printf(" Use only one of -xl, -b and -Q.\n");
    printf("If none of these is present, the");
    printf(" program attempts to guess a \n");
    printf("suitable note length.\n");
    exit(0);
  };
  trackno = 0;
  track[trackno].texthead = NULL;
  track[trackno].texttail = NULL;
  initfuncs();
  playinghead = NULL;
  playingtail = NULL;
  xunit = 0;
  karaoke = 0;
  Mf_getc = filegetc;
  mfread();
  fclose(F);
  maintrack = 0;
  while ((track[maintrack].notes == 0) && (maintrack < trackcount)) {
    maintrack = maintrack + 1;
  };
  if (track[maintrack].notes == 0) {
    fatal_error("MIDI file has no notes!");
  };
  /* compute dtnext for each note */
  for (j=0; j<trackcount; j++) {
    postprocess(j);
  };
  fprintf(outhandle,"X: 1\n"); 
  fprintf(outhandle,"T: \n"); 
  fprintf(outhandle,"M: %d/%d\n", asig, bsig);
  fprintf(outhandle,"L: 1/%d\n", unitlen); 
  barsize = asig*unitlen/bsig;
  if (Qval != 0) {
    xunit = mf_sec2ticks((60.0*4.0)/((float)(Qval*unitlen)), division, tempo);
  };
  if (bars > 0) {
    xunit = (int) (track[maintrack].notes/(bars*barsize));
  };
  if (xunit == 0) {
    guesslengths(maintrack);
  };
  printQ();
  if(summary > 0) printf("xunit is set to %d clock ticks\n",xunit);
  for (j=0; j<trackcount; j++) {
    quantize(j, xunit);
  };
  if (extracta) {
    anacrusis = findana(maintrack, barsize*2);
    fprintf(outhandle,"%%beats in anacrusis = %d\n", anacrusis);
  };
  if (guessa) {
    anacrusis = guessana(barsize*2);
    fprintf(outhandle,"%%beats in anacrusis = %d\n", anacrusis);
  };
  if (keysig == -50) {
    keysig = findkey(maintrack);
  };
  setupkey(keysig);
  /* scannotes(maintrack); */
  if (trackcount > 1) {
    voiceno = 1;
    for (j=0; j<trackcount; j++) {
      freshline();
      if (track[j].notes > 0) {
        fprintf(outhandle,"V:%d\n", voiceno);
        voiceno = voiceno + 1;
        if (track[j].drumtrack) {
          fprintf(outhandle, "%%%%MIDI channel 10\n");
        };
      };
      printtrack(j, barsize*2, anacrusis);
    };
  } else {
    printtrack(maintrack, barsize*2, anacrusis);
  };
  /* scannotes(maintrack); */
  /* free up data structures */
  for (j=0; j< trackcount; j++) {
    struct listx* this;
    struct listx* x;
    struct tlistx* tthis;
    struct tlistx* tx;

    this = track[j].head;
    while (this != NULL) {
      free(this->note);
      x = this->next ;
      free(this);
      this = x;
    };
    tthis = track[j].texthead;
    while (tthis != NULL) {
      free(tthis->text);
      tx = tthis->next;
      free(tthis);
      tthis = tx;
    };
  };
  fclose(outhandle);
  return(0);
}

FILE *
efopen(name,mode)
char *name;
char *mode;
{
    FILE *f;

    if ( (f=fopen(name,mode)) == NULL ) {
      char msg[80];

      sprintf(msg,"Error - Cannot open file %s",name);
      fatal_error(msg);
    }
    return(f);
}

int error(s)
char *s;
{
    fprintf(stderr,"Error: %s\n",s);
}

/* The following C routines are required by midifilelib.  */
/* They specify the action to be taken when various items */
/* are encountered in the MIDI.                           */

int txt_header(xformat,ntrks,ldivision)
int xformat, ntrks, ldivision;
{
    division = ldivision; 
    format = xformat;
    if (format != 0) {
      fprintf(outhandle,"%% format %d file %d tracks\n", format, ntrks);
    };
}

int txt_trackstart()
{
  laston = 0L;
  track[trackno].notes = 0;
  track[trackno].head = NULL;
  track[trackno].tail = NULL;
  track[trackno].texthead = NULL;
  track[trackno].texttail = NULL;
  track[trackno].tracklen = Mf_currtime;
  track[trackno].drumtrack = 0;
}

int txt_trackend()
{
  /* check for unfinished notes */
  if (playinghead != NULL) {
    printf("Error in MIDI file - notes still on at end of track!\n");
  };
  track[trackno].tracklen = Mf_currtime - track[trackno].tracklen;
  trackno = trackno + 1;
  trackcount = trackcount + 1;
}

int txt_noteon(chan,pitch,vol)
int chan, pitch, vol;
{
  if ((xchannel == -1) || (chan == xchannel)) {
    if (vol != 0) {
      addnote(pitch, chan, vol);
    } else {
      notestop(pitch, chan);
    };
  };
}

int txt_noteoff(chan,pitch,vol)
int chan, pitch, vol;
{
  if ((xchannel == -1) || (chan == xchannel)) {
    notestop(pitch, chan);
  };
}

int txt_pressure(chan,pitch,press)
int chan, pitch, press;
{
}

int txt_parameter(chan,control,value)
int chan, control, value;
{
}

int txt_pitchbend(chan,msb,lsb)
int chan, msb, lsb;
{
}

int txt_program(chan,program)
int chan, program;
{
/*
  sprintf(textbuff, "%%%%MIDI program %d %d",
         chan+1, program);
*/
  sprintf(textbuff, "%%%%MIDI program %d", program);
  addtext(textbuff);
}

int txt_chanpressure(chan,press)
int chan, press;
{
}

int txt_sysex(leng,mess)
int leng;
char *mess;
{
}

int txt_metamisc(type,leng,mess)
int type, leng;
char *mess;
{
}

int txt_metaspecial(type,leng,mess)
int type, leng;
char *mess;
{
}

int txt_metatext(type,leng,mess)
int type, leng;
char *mess;
{ 
  static char *ttype[] = {
    NULL,
    "Text Event",        /* type=0x01 */
    "Copyright Notice",    /* type=0x02 */
    "Sequence/Track Name",
    "Instrument Name",    /* ...     */
    "Lyric",
    "Marker",
    "Cue Point",        /* type=0x07 */
    "Unrecognized"
  };
  int unrecognized = (sizeof(ttype)/sizeof(char *)) - 1;
  unsigned char c;
  int n;
  char *p = mess;
  char *buff;
  char buffer2[BUFFSIZE];

  if ((type < 1)||(type > unrecognized))
      type = unrecognized;
  buff = textbuff;
  for (n=0; n<leng; n++) {
    c = *p++;
    if (buff - textbuff < BUFFSIZE - 6) {
      sprintf(buff, 
           (isprint(c)||isspace(c)) ? "%c" : "\\0x%02x" , c);
      buff = buff + strlen(buff);
    };
  }
  if (strncmp(textbuff, "@KMIDI KARAOKE FILE", 14) == 0) {
    karaoke = 1;
  } else {
    if ((karaoke == 1) && (*textbuff != '@')) {
      addtext(textbuff);
    } else {
      if (leng < BUFFSIZE - 3) {
        sprintf(buffer2, "%%%s", textbuff);
        addtext(buffer2);
      };
    };
  };
}

int txt_metaseq(num)
int num;
{  
  sprintf(textbuff, "%%Meta event, sequence number = %d",num);
  addtext(textbuff);
}

int txt_metaeot()
/* Meta event, end of track */
{
}

int txt_keysig(sf,mi)
char sf, mi;
{
  int accidentals;
  sprintf(textbuff, 
         "%% MIDI Key signature, sharp/flats=%d  minor=%d",
          (int) sf, (int) mi);
  addtext(textbuff);
  if (summary <= 0) return;
  accidentals = (int) sf;
  if (accidentals <0 )
    {accidentals = -accidentals;
     printf("key signature: %d flats\n", accidentals);
    }
  else
     printf("key signature : %d sharps\n", accidentals);
}

int txt_tempo(ltempo)
long ltempo;
{
    tempo = ltempo;
}

int txt_timesig(nn,dd,cc,bb)
int nn, dd, cc, bb;
{
  int denom = 1;
  while ( dd-- > 0 )
    denom *= 2;
  sprintf(textbuff, 
          "%% Time signature=%d/%d  MIDI-clocks/click=%d  32nd-notes/24-MIDI-clocks=%d", 
    nn,denom,cc,bb);
  addtext(textbuff);
  if (summary>0) printf("Time signature = %d/%d\n",nn,denom);
  if (extractm) {
    asig = nn;
    bsig = denom;
    if ((asig*4)/bsig >= 3) {
      unitlen =8;
    } else {
      unitlen = 16;
    };
  };
  if (extractl) {
    xunit = (division*bb*4)/(8*unitlen);
  };
}


int txt_smpte(hr,mn,se,fr,ff)
int hr, mn, se, fr, ff;
{
}

int txt_arbitrary(leng,mess)
char *mess;
int leng;
{
}

void initfuncs()
{
    Mf_error = error;
    Mf_header =  txt_header;
    Mf_trackstart =  txt_trackstart;
    Mf_trackend =  txt_trackend;
    Mf_noteon =  txt_noteon;
    Mf_noteoff =  txt_noteoff;
    Mf_pressure =  txt_pressure;
    Mf_parameter =  txt_parameter;
    Mf_pitchbend =  txt_pitchbend;
    Mf_program =  txt_program;
    Mf_chanpressure =  txt_chanpressure;
    Mf_sysex =  txt_sysex;
    Mf_metamisc =  txt_metamisc;
    Mf_seqnum =  txt_metaseq;
    Mf_eot =  txt_metaeot;
    Mf_timesig =  txt_timesig;
    Mf_smpte =  txt_smpte;
    Mf_tempo =  txt_tempo;
    Mf_keysig =  txt_keysig;
    Mf_seqspecific =  txt_metaspecial;
    Mf_text =  txt_metatext;
    Mf_arbitrary =  txt_arbitrary;
}
