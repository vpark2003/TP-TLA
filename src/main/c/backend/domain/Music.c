#include "Music.h"
#include <stdio.h>
#include <string.h>

/* TABLES */

static const ChordQuality _chordQualities[] = {
	/* Triads. */
	{ "", { 0, 4, 7 }, 3 },
	{ "M", { 0, 4, 7 }, 3 },
	{ "maj", { 0, 4, 7 }, 3 },
	{ "m", { 0, 3, 7 }, 3 },
	{ "min", { 0, 3, 7 }, 3 },
	{ "dim", { 0, 3, 6 }, 3 },
	{ "aug", { 0, 4, 8 }, 3 },
	{ "+", { 0, 4, 8 }, 3 },
	{ "sus", { 0, 5, 7 }, 3 },
	{ "sus4", { 0, 5, 7 }, 3 },
	{ "sus2", { 0, 2, 7 }, 3 },
	{ "5", { 0, 7 }, 2 },
	/* Sixths. */
	{ "6", { 0, 4, 7, 9 }, 4 },
	{ "m6", { 0, 3, 7, 9 }, 4 },
	/* Sevenths. */
	{ "7", { 0, 4, 7, 10 }, 4 },
	{ "maj7", { 0, 4, 7, 11 }, 4 },
	{ "M7", { 0, 4, 7, 11 }, 4 },
	{ "m7", { 0, 3, 7, 10 }, 4 },
	{ "min7", { 0, 3, 7, 10 }, 4 },
	{ "mmaj7", { 0, 3, 7, 11 }, 4 },
	{ "mM7", { 0, 3, 7, 11 }, 4 },
	{ "dim7", { 0, 3, 6, 9 }, 4 },
	{ "m7b5", { 0, 3, 6, 10 }, 4 },
	{ "aug7", { 0, 4, 8, 10 }, 4 },
	{ "+7", { 0, 4, 8, 10 }, 4 },
	{ "7#5", { 0, 4, 8, 10 }, 4 },
	{ "7b5", { 0, 4, 6, 10 }, 4 },
	{ "7sus4", { 0, 5, 7, 10 }, 4 },
	/* Extensions. */
	{ "add9", { 0, 4, 7, 14 }, 4 },
	{ "madd9", { 0, 3, 7, 14 }, 4 },
	{ "9", { 0, 4, 7, 10, 14 }, 5 },
	{ "maj9", { 0, 4, 7, 11, 14 }, 5 },
	{ "m9", { 0, 3, 7, 10, 14 }, 5 },
	{ "7b9", { 0, 4, 7, 10, 13 }, 5 },
	{ "7#9", { 0, 4, 7, 10, 15 }, 5 },
	{ "11", { 0, 4, 7, 10, 14, 17 }, 6 },
	{ "m11", { 0, 3, 7, 10, 14, 17 }, 6 },
	{ "13", { 0, 4, 7, 10, 14, 21 }, 6 },
	{ "maj13", { 0, 4, 7, 11, 14, 21 }, 6 },
	{ "m13", { 0, 3, 7, 10, 14, 21 }, 6 }
};

/**
 * General MIDI programs (0-based).
 *
 * @see https://www.midi.org/specifications-old/item/gm-level-1-sound-set
 */
static const Instrument _instruments[] = {
	{ "piano", 0, false },
	{ "epiano", 4, false },
	{ "harpsichord", 6, false },
	{ "vibraphone", 11, false },
	{ "marimba", 12, false },
	{ "organ", 16, false },
	{ "accordion", 21, false },
	{ "guitar", 24, false },
	{ "acoustic_guitar", 25, false },
	{ "electric_guitar", 27, false },
	{ "acoustic_bass", 32, true },
	{ "bass", 33, true },
	{ "violin", 40, false },
	{ "viola", 41, false },
	{ "cello", 42, false },
	{ "contrabass", 43, true },
	{ "harp", 46, false },
	{ "strings", 48, false },
	{ "choir", 52, false },
	{ "trumpet", 56, false },
	{ "trombone", 57, false },
	{ "brass", 61, false },
	{ "sax", 65, false },
	{ "clarinet", 71, false },
	{ "flute", 73, false },
	{ "synth", 80, false },
	{ "pad", 88, false }
};

typedef struct {
	const char * note;
	const int accidentals;
} KeySignature;

static const KeySignature _majorKeys[] = {
	{ "Cb", -7 }, { "Gb", -6 }, { "Db", -5 }, { "Ab", -4 }, { "Eb", -3 },
	{ "Bb", -2 }, { "F", -1 }, { "C", 0 }, { "G", 1 }, { "D", 2 },
	{ "A", 3 }, { "E", 4 }, { "B", 5 }, { "F#", 6 }, { "C#", 7 }
};

static const KeySignature _minorKeys[] = {
	{ "Ab", -7 }, { "Eb", -6 }, { "Bb", -5 }, { "F", -4 }, { "C", -3 },
	{ "G", -2 }, { "D", -1 }, { "A", 0 }, { "E", 1 }, { "B", 2 },
	{ "F#", 3 }, { "C#", 4 }, { "G#", 5 }, { "D#", 6 }, { "A#", 7 }
};

#define LENGTH(array) (sizeof(array) / sizeof((array)[0]))

static const int _majorScale[] = { 0, 2, 4, 5, 7, 9, 11 };
/** Natural minor plus the raised 7th degree (harmonic minor). */
static const int _minorScale[] = { 0, 2, 3, 5, 7, 8, 10, 11 };

/* PUBLIC FUNCTIONS */

int pitchClassOf(const char * note) {
	static const int natural[] = { 9, 11, 0, 2, 4, 5, 7 }; /* A B C D E F G */
	if (note == NULL || note[0] < 'A' || 'G' < note[0]) {
		return INVALID_PITCH_CLASS;
	}
	int pitchClass = natural[note[0] - 'A'];
	if (note[1] == '#') {
		pitchClass += 1;
	}
	else if (note[1] == 'b') {
		pitchClass += 11;
	}
	else if (note[1] != '\0') {
		return INVALID_PITCH_CLASS;
	}
	return pitchClass % 12;
}

const ChordQuality * findChordQuality(const char * quality) {
	for (unsigned int k = 0; k < LENGTH(_chordQualities); ++k) {
		if (strcmp(_chordQualities[k].name, quality) == 0) {
			return &_chordQualities[k];
		}
	}
	return NULL;
}

const Instrument * findInstrument(const char * name) {
	for (unsigned int k = 0; k < LENGTH(_instruments); ++k) {
		if (strcmp(_instruments[k].name, name) == 0) {
			return &_instruments[k];
		}
	}
	return NULL;
}

const Instrument * defaultInstrument() {
	return &_instruments[0];
}

void listInstruments(char * buffer, const unsigned int size) {
	unsigned int used = 0;
	buffer[0] = '\0';
	for (unsigned int k = 0; k < LENGTH(_instruments) && used < size; ++k) {
		used += snprintf(buffer + used, size - used, "%s%s", k == 0 ? "" : ", ", _instruments[k].name);
	}
}

bool keySignatureOf(const char * note, const Mode mode, int * accidentals) {
	const KeySignature * keys = mode == MAJOR_MODE ? _majorKeys : _minorKeys;
	const unsigned int length = mode == MAJOR_MODE ? LENGTH(_majorKeys) : LENGTH(_minorKeys);
	for (unsigned int k = 0; k < length; ++k) {
		if (strcmp(keys[k].note, note) == 0) {
			*accidentals = keys[k].accidentals;
			return true;
		}
	}
	return false;
}

static bool _belongsToScale(const int tonic, const Mode mode, const int pitchClass) {
	const int * scale = mode == MAJOR_MODE ? _majorScale : _minorScale;
	const unsigned int length = mode == MAJOR_MODE ? LENGTH(_majorScale) : LENGTH(_minorScale);
	const int degree = (pitchClass - tonic + 12) % 12;
	for (unsigned int k = 0; k < length; ++k) {
		if (scale[k] == degree) {
			return true;
		}
	}
	return false;
}

bool isDiatonicChord(const char * keyNote, const Mode mode, const Chord * chord) {
	if (chord->type == NO_CHORD_SYMBOL) {
		return true;
	}
	const int tonic = pitchClassOf(keyNote);
	const int root = pitchClassOf(chord->root);
	const ChordQuality * quality = findChordQuality(chord->quality);
	if (tonic == INVALID_PITCH_CLASS || root == INVALID_PITCH_CLASS || quality == NULL) {
		return true;
	}
	for (unsigned int k = 0; k < quality->length; ++k) {
		if (!_belongsToScale(tonic, mode, (root + quality->intervals[k]) % 12)) {
			return false;
		}
	}
	if (chord->bass != NULL && !_belongsToScale(tonic, mode, pitchClassOf(chord->bass))) {
		return false;
	}
	return true;
}
