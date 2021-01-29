abcMIDI :   abc <-> MIDI conversion utilities

midi2abc version 2.4
abc2midi version 1.24
abc2abc  version 1.19
yaps     version 1.12

version number for the abcMIDI package: 36
This version number will be updated whenever any of the component
programs or documentation changes and is mainly for the benefit of
people packaging the abcMIDI programs for Linux.

24th January 2002

Copyright James Allwright
J.R.Allwright@westminster.ac.uk
University of Westminster,
London, UK

This is free software. You may copy and re-distribute it under the terms of 
the GNU General Public License version 2 or later, which is available from
the Free Software Foundation (and elsewhere). 

This package is to be found on the web at

http://abc.sourceforge.net/abcMIDI/

These programs make use of the 'midifilelib' public domain MIDI file utilities,
available from

http://www.harmony-central.com/MIDI/midifilelib.tar.gz

If you have the source distribution and intend to re-compile the code,
read the file coding.txt.
---------------------------------------------------------------------
midi2abc - program to convert MIDI format files to abc notation. 

This program takes a MIDI format file and converts it to something as close
as possible to abc text format. The user then has to add text fields not
present in the MIDI header and possibly tidy up the abc note output.

Features :

* The key is chosen so as to minimize the number of accidentals. 
Alternatively, the user can specify the key numerically (a positive number
is the number of sharps, a negative number is minus the number of flats).
* Note length can be set by specifiying the total number of bars or the 
tempo of the piece. Alternatively the note length can be read from the file.
However, by default it is deduced in a heuristic manner from the inter-note 
distances.  This means that you do not have to use the MIDI clock as a 
metronome when playing in a tune from a keyboard. 
* Barlines are automatically inserted. The user specifies the number of
measures in the anacrusis before the first barline and the time signature.
* The program can guess how many beats there should be in the anacrusis,
either by looking for the first strong note or minimizing the number of
notes split by a tie across a barline.
* Where a note extends beyond a bar break, it is split into two tied notes.
* The output has 4 bars per line.
* Enough accidental signs are put in the music to ensure that no pitch
errors occur if a barline is added or deleted.
* The program attempts to group notes sensibly in each bar.
* Triplets and broken rhythm (a>b) are supported.
* Chords are identified.
* Text information from the original MIDI file is included as comments.
* The -c option can be used to select only 1 MIDI channel. Events on 
other channels are ignored.

What midi2abc does not do :

* Supply tune title, composer or any other field apart from X: , K:, Q:, M:
and L: - these must be added by hand afterwards, though they may have been
included in the text of the MIDI file.
* Support duplets, quadruplets, other esoteric features.
* Support mid-tune key or meter changes.
* Deduce repeats. The output is just the notes in the input file.
* Recover an abc tune as supplied to abc2midi. However, if you want to
do this, "midi2abc -xm -xl -xa -f file.mid" comes close.

midi2abc 
  usage :
midi2abc <options>
         -a <beats in anacrusis>
         -xa  extract anacrusis from file (find first strong note)
         -ga  guess anacrusis (minimize ties across bars)
         -m <time signature>
         -xm  extract time signature from file
         -xl  extract absolute note lengths from file
         -b <bars wanted in output>
         -Q <tempo in quarter-notes per minute>
         -k <key signature> -6 to 6 sharps
         -c <channel>
         [-f] <input file>
         -o <output file>
         -s do not discard very short notes
         -sr do not notate a short rest after a note
         -sum summary
         -nt do not look for triplets or broken rhythm
 Use only one of -xl, -b and -Q.
If none of these is present, the program attempts to guess a 
suitable note length.

The output of midi2abc is printed to the screen. To save it to a file, use
the redirection operator.

e.g.

midi2abc -f file.mid > file.abc

If the MIDI file is computer-generated, you may be able to extract the time
signature from it using the -xm option. Otherwise you should specify it with
-m. Allowable time signatures are C, 4/4, 3/8, 2/4 and so on.

If the tune has an anacrusis, you should specify the number of beats in
it using the -a option. 

-------------------------------------------------------------------------

abc2midi  - converts abc file to MIDI file(s).
Usage : abc2midi <abc file> [reference number] [-c] [-v] [-o filename]
        [-t] [-n <value>]
        [reference number] selects a tune
        -c  selects checking only
        -v  selects verbose option
        -o <filename>  selects output filename
        -t selects filenames derived from tune titles
        -n <limit> set limit for length of filename stem
 The default action is to write a MIDI file for each abc tune
 with the filename <stem>N.mid, where <stem> is the filestem
 of the abc file and N is the tune reference number. If the -o
 option is used, only one file is written. This is the tune
 specified by the reference number or, if no reference number
 is given, the first tune in the file.

Features :

* Broken rythms (>, <), chords, n-tuples, slurring, ties, staccatto notes,
repeats, in-tune tempo/length/meter changes are all supported.

* R:hornpipe or r:hornpipe is recognized and note timings are adjusted to
give a broken rythm (ab is converted to a>b).

* Most errors in the abc input will generate a suitable error message in
the output and the converter keeps going.

* Comments and text fields in the abc source are converted to text events
in the MIDI output

* If guitar chords are present, they are used to generate an accompaniment
in the MIDI output.

* If there are mis-matched repeat signs in the abc, the program attempts to
fix them. However, it will not attempt this if a multi-part tune 
description has been used or if multiple voices are in use.

* Karaoke MIDI files can be generated by using the w: field to include 
lyrics.

* There are some extensions to the abc syntax of the form

%%MIDI channel n

These control channel and program selection, transposing and various
other features of abc2midi. See the file abcguide.txt for more
details.

Bugs and Limitations :

* No field is inherited from above the X: field of the tune.

* Where an accidental is applied to a tied note that extends across
  a barline, abc2midi requires that the note beyond the barline must 
  be explicitly given an accidental e.g.

  ^F- | F     - will be reported as an error.
  ^F- | ^F    - will produce a tied ^F note.

  It is common to see no accidental shown when this occurs in published 
  printed music.

-------------------------------------------------------------------------
abc2abc

Usage: abc2abc <filename> [-s] [-n X] [-b] [-r] [-e] [-t X]
       [-u] [-d] [-v] [-V X] [-X n]
  -s for new spacing
  -n X to re-format the abc with a new linebreak every X bars
  -b to remove bar checking
  -r to remove repeat checking
  -e to remove all error reports
  -t X to transpose X semitones
  -u to update notation ([] for chords and () for slurs)
  -d to notate with doubled note lengths
  -v to notate with halved note lengths
  -V X to output only voice X
  -X n renumber the all X: fields as n, n+1, ..

A simple abc checker/re-formatter/transposer. If the -n option is selected, 
error checking is turned off. 

If you want to check an abc tune, it is recommended that you use abc2midi 
with the -c option as this performs extra checks that abc2abc does not do.

The output of abc2abc is printed to the screen. To save it to a file, use
the redirection operator.

e.g.

abc2abc file.abc -t 2 > newfile.abc

Known problems:
* When using the -n option on a program with lyrics, a barline in a w:
  field may be carried forward to the next w: field.
-------------------------------------------------------------------------

mftext - MIDI file to text

This gives a verbose description of what is in a MIDI file. You may wish
to use it to check the output from abc2midi. It is part of the original
midifilelib distribution.

-------------------------------------------------------------------------
YAPS
----
YAPS is an abc to PostScript converter which can be used as an alternative
to abc2ps. See the file yaps.txt for a more detailed description of this
program.

-------------------------------------------------------------------------
A Short Explanation of MIDI
---------------------------
MIDI stands for "Musical Instrument Digital Interface". MIDI was originally
designed to connect a controller, such as a piano-style keyboard, to a
synthesizer. A MIDI cable is similar to a serial RS232 cable but uses
different voltage levels and an unusual baud rate (31250 baud). The MIDI
standard also defines the meaning of the digital signals sent down the 
cable; pressing and releasing a key produces 2 of these signals, a 
"note on" followed by a "note off". 

There is an additional standard for MIDI files, which describes how to 
record these signals together with the time when each signal was produced. 
This allows a complete performance to be recorded in a compact digital 
form. It is also possible for a computer to write a MIDI file which can 
be played back in exactly the same way as a MIDI file of a recorded 
performance. This is what abc2midi does.

-------------------------------------------------------------------------
Note: DPMI server for DOS executables

If you have downloaded the executables compiled using DJGPP, you may get
an error message saying that a DPMI (Dos Protected Mode Interface) server
is needed. If you can run the programs from a DOS window within Windows,
this may solve the problem. Alternatively, download the DPMI server
recommended for DJGPP, called CWSDPMI.EXE. This needs to be on your path
for the executables to run.
-------------------------------------------------------------------------
Bug reports

Please report any bugs you find in abc2midi, midi2abc or abc2abc to 
J.R.Allwright@westminster.ac.uk (preferably with an example so that I
can replicate the problem). Better still, send me a patch to fix the 
problem! If you add your own features to the code that other people 
might want to use then let me know - I may or may not want to add them 
to the official version, but I'll put up a link to your version. So
far I have been maintaining the code, but I don't guarantee anything.
