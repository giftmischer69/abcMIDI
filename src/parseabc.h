/* parseabc.h - interface file for abc parser */
/* used by abc2midi, abc2abc and yaps */

/* abc.h must be #included before this file */
/* functions and variables provided by parseabc.c */
#ifndef KANDR
extern int readnump(char **p);
extern int readnumf(char *num);
extern void skipspace(char **p);
extern int readsnumf(char *s);
extern void readstr(char out[], char **in, int limit);
extern int getarg(char *option, int argc, char *argv[]);
extern int *checkmalloc(int size);
extern char *addstring(char *s);
extern char *lookup_abbreviation(char symbol);
#else
extern int readnump();
extern int readnumf();
extern void skipspace();
extern int readsnumf();
extern void readstr();
extern int getarg();
extern int *checkmalloc();
extern char *addstring();
extern char *lookup_abbreviation();
#endif
extern void parseron();
extern void parseroff();

extern int lineno;

/* event_X() routines - these are called from parseabc.c       */
/* the program that uses the parser must supply these routines */
#ifndef KANDR
extern void event_init(int argc, char *argv[], char **filename);
extern void event_text(char *s);
extern void event_reserved(char p);
extern void event_tex(char *s);
extern void event_linebreak(void);
extern void event_startmusicline(void);
extern void event_endmusicline(char endchar);
extern void event_eof(void);
extern void event_comment(char *s);
extern void event_specific(char *package, char *s);
extern void event_startinline(void);
extern void event_closeinline(void);
extern void event_field(char k, char *f);
extern void event_words(char *p, int continuation);
extern void event_part(char *s);
extern void event_voice(int n, char *s);
extern void event_length(int n);
extern void event_blankline(void);
extern void event_refno(int n);
extern void event_tempo(int n, int a, int b, int rel, char *pre, char *post);
extern void event_timesig(int n, int m, int dochecking);
extern void event_info_key(char *key, char *value);
extern void event_info(char *s);
extern void event_key(int sharps, char *s, int minor, 
               char modmap[7], int modmul[7],
               int gotkey, int gotclef, char *clefname,
               int octave, int transpose, int gotoctave, int gottranspose);
extern void event_graceon(void);
extern void event_graceoff(void);
extern void event_rep1(void);
extern void event_rep2(void);
extern void event_playonrep(char *s);
extern void event_tie(void);
extern void event_slur(int t);
extern void event_sluron(int t);
extern void event_sluroff(int t);
extern void event_rest(int n,int m);
extern void event_mrest(int n,int m);
extern void event_bar(int type, char *replist);
extern void event_space(void);
extern void event_lineend(char ch, int n);
extern void event_broken(int type, int mult);
extern void event_tuple(int n, int q, int r);
extern void event_chord(void);
extern void event_chordon(void);
extern void event_chordoff(void);
extern void event_instruction(char *s);
extern void event_gchord(char *s);
extern void event_note(int decorators[DECSIZE], char accidental, int mult, 
                       char note, int xoctave, int n, int m);
extern void event_abbreviation(char symbol, char *string, char container);
#else
extern void event_init();
extern void event_text();
extern void event_reserved();
extern void event_tex();
extern void event_linebreak();
extern void event_startmusicline();
extern void event_endmusicline();
extern void event_eof();
extern void event_comment();
extern void event_specific();
extern void event_startinline();
extern void event_closeinline();
extern void event_field();
extern void event_words();
extern void event_part();
extern void event_voice();
extern void event_length();
extern void event_blankline();
extern void event_refno();
extern void event_tempo();
extern void event_timesig();
extern void event_info_key();
extern void event_info();
extern void event_key();
extern void event_graceon();
extern void event_graceoff();
extern void event_rep1();
extern void event_rep2();
extern void event_playonrep();
extern void event_tie();
extern void event_slur();
extern void event_sluron();
extern void event_sluroff();
extern void event_rest();
extern void event_mrest();
extern void event_bar();
extern void event_space();
extern void event_lineend();
extern void event_broken();
extern void event_tuple();
extern void event_chord();
extern void event_chordon();
extern void event_chordoff();
extern void event_instruction();
extern void event_gchord();
extern void event_note();
extern void event_abbreviation();
#endif
