/* genmidi.h - part of abc2midi */
/* function prototypes for functions in genmidi.c used elsewhere */

#ifndef KANDR
/* functions required by store.c */
extern void reduce(int* a, int* b);
extern void set_meter(int a, int b);
extern void set_gchords(char *s);
extern void set_drums(char *s);
extern void addunits(int a, int b);
/* required by queues.c */
extern void midi_noteoff(long delta_time, int pitch, int chan);
extern void progress_sequence(int i);
#else
/* functions required by store.c */
extern void reduce();
extern void set_meter();
extern void set_gchords();
extern void addunits();
/* required by queues.c */
extern void midi_noteoff();
extern void progress_sequence();
#endif

/* some definitions formerly in tomidi.c */
#define DIV 480
#define MAXPARTS 100
#define MAXCHORDNAMES 80
