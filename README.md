# abcMIDI

the purpose of this repository is to distribute built abcMIDI packages for
windows and linux

[download release](https://github.com/giftmischer69/abcMIDI/releases/download/1.0.0/abcMIDI.zip)

mirror of: http://abc.sourceforge.net/abcMIDI/original/

---

The abcMIDI package contains the following :
- abc2midi - a program to convert abc notation to MIDI files.
- midi2abc - a program to produce crude abc from a MIDI file.
- abc2abc - a utility to do transposition, error checking and re-formatting on an abc file.
- yaps - (NEW) utility to convert abc to PostScript based on Michael Methfessel's PostScript library for abc2ps.

---

current versions :
- midi2abc - version 2.4
- abc2midi - version 1.24
- abc2abc - version 1.19
- yaps - version 1.12

---

## Pre-Installation Dependencies:
- any kind of [conda](https://docs.conda.io/en/latest/index.html) installation, e.g.:
  - [miniconda](https://docs.conda.io/en/latest/miniconda.html) (minimal, recommended) or
  - [anaconda](https://www.anaconda.com/products/individual) or

- download this repo, e.g.
  - `git clone https://github.com/giftmischer69/premix`
  - [download zip](https://github.com/giftmischer69/premix/archive/main.zip)

## install:
Ubuntu (tested with Pop_OS!)
- `chmod +x install.cmd`
- `./install.cmd`

Windows
- `install.cmd`

## build:
Ubuntu (tested with Pop_OS!)
- `./build.cmd`

Windows
- `build.cmd`
