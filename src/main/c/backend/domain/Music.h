#ifndef MUSIC_HEADER
#define MUSIC_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include <stdbool.h>

/**
 * Domain knowledge of B#: notes, chord qualities, instruments and keys. Both
 * the semantic analysis (to validate) and the code generation (to build the
 * MIDI notes) share these tables.
 */

#define INVALID_PITCH_CLASS -1
#define MAX_CHORD_INTERVALS 7

typedef struct {
	/** The quality as written after the root (e.g., "", "m", "maj7"). */
	const char * name;
	/** Semitones above the root of every chord tone (the root is 0). */
	const int intervals[MAX_CHORD_INTERVALS];
	const unsigned int length;
} ChordQuality;

typedef struct {
	/** The name used in "track <name> { ... }". */
	const char * name;
	/** The General MIDI program number (0-127). */
	const unsigned char program;
	/** If true, the instrument only plays the bass note of every chord. */
	const bool bassLine;
} Instrument;

/**
 * The pitch class of a note (C=0, C#=1, ..., B=11), or INVALID_PITCH_CLASS.
 */
int pitchClassOf(const char * note);

/**
 * The chord quality with that exact name, or NULL if it isn't supported.
 */
const ChordQuality * findChordQuality(const char * quality);

/**
 * The instrument with that exact name, or NULL if it isn't supported.
 */
const Instrument * findInstrument(const char * name);

/**
 * The default instrument for sections without tracks.
 */
const Instrument * defaultInstrument();

/**
 * Writes the list of supported instruments, separated by commas, into the
 * buffer (used in error messages).
 */
void listInstruments(char * buffer, const unsigned int size);

/**
 * The amount of sharps (positive) or flats (negative) of the key signature,
 * as required by the MIDI key-signature meta-event. Returns false if the key
 * is theoretical (e.g., "G# major", which would need 8 sharps).
 */
bool keySignatureOf(const char * note, const Mode mode, int * accidentals);

/**
 * True if every note of the chord belongs to the key. For minor keys, the
 * harmonic minor scale (raised 7th degree) is also accepted, so the dominant
 * chord (e.g., E7 in A minor) is considered diatonic.
 */
bool isDiatonicChord(const char * keyNote, const Mode mode, const Chord * chord);

#endif
