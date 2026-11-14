#include "Generator.h"
#include <string.h>

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

void _shutdownGeneratorModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: Generator...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeGeneratorModule() {
	_logger = createLogger("Generator");
	return _shutdownGeneratorModule;
}

/**
 * PRIVATE TYPES AND CONSTANTS
 *
 * The output is a Standard MIDI File, format 1 (multiple tracks played in
 * parallel). The first track (conductor) holds the song metadata, the tempo,
 * the time signature and the key signature. Then, there is one track per
 * distinct instrument used in the song, each one in its own channel.
 *
 * @see https://www.midi.org/specifications/file-format-specifications/standard-midi-files
 */

/** Ticks per quarter note. */
#define PPQ 480
#define PERCUSSION_CHANNEL 9
/** MIDI has 16 channels, but channel 10 is reserved for percussion. */
#define MAX_INSTRUMENTS 15
/** The MIDI tempo is stored in 24 bits (microseconds per quarter note). */
#define MAX_MIDI_TEMPO 0xFFFFFFUL
/** Guards the pattern expansion against cyclic usages. */
#define MAX_PATTERN_DEPTH 64
#define CHORD_VELOCITY 80
#define BASS_VELOCITY 90
/** MIDI note of C3: chords are voiced from here upwards. */
#define CHORD_BASE_NOTE 48
/** MIDI note of E1: the bass line moves between E1 and D#2. */
#define BASS_LINE_BASE_NOTE 28
/** MIDI note of C2: bass note of slash chords played by harmonic instruments. */
#define SLASH_BASS_BASE_NOTE 36

/** Default values when the song doesn't declare them. */
#define DEFAULT_TEMPO 120
#define DEFAULT_NUMERATOR 4
#define DEFAULT_DENOMINATOR 4

typedef struct {
	unsigned char * bytes;
	size_t length;
	size_t capacity;
} ByteBuffer;

/** A note event, in absolute time (ticks since the beginning). */
typedef struct {
	unsigned long tick;
	unsigned char status;
	unsigned char note;
	unsigned char velocity;
} NoteEvent;

typedef struct {
	ByteBuffer buffer;
	const Instrument * instrument;
	unsigned char channel;
	/** Absolute time of the last event written, to compute the deltas. */
	unsigned long lastTick;
	/**
	 * Note events are collected first and sorted before being written, because
	 * two parallel tracks of the same section may share a MIDI track.
	 */
	NoteEvent * events;
	size_t eventCount;
	size_t eventCapacity;
} MidiTrack;

/** The global configuration of the song, read from the AST. */
typedef struct {
	char * name;
	char * author;
	char * copyright;
	int tempo;
	int numerator;
	int denominator;
	Key * key;
} SongSettings;

typedef struct {
	SongSettings settings;
	SymbolTable * symbolTable;
	MidiTrack tracks[MAX_INSTRUMENTS];
	unsigned int trackCount;
	unsigned long beatTicks;
	unsigned long bars;
	bool failed;
} GenerationContext;

/* BYTE BUFFER */

static void _writeByte(ByteBuffer * buffer, const unsigned char byte) {
	if (buffer->length == buffer->capacity) {
		buffer->capacity = buffer->capacity == 0 ? 256 : 2 * buffer->capacity;
		buffer->bytes = realloc(buffer->bytes, buffer->capacity);
	}
	buffer->bytes[buffer->length++] = byte;
}

static void _writeBytes(ByteBuffer * buffer, const void * bytes, const size_t length) {
	for (size_t k = 0; k < length; ++k) {
		_writeByte(buffer, ((const unsigned char *) bytes)[k]);
	}
}

/** Big-endian integer of "size" bytes. */
static void _writeInteger(ByteBuffer * buffer, const unsigned long value, const unsigned int size) {
	for (int k = (int) size - 1; 0 <= k; --k) {
		_writeByte(buffer, (value >> (8 * k)) & 0xFF);
	}
}

/** Variable-length quantity (7 bits per byte, MSB = "more bytes follow"). */
static void _writeVariableLength(ByteBuffer * buffer, unsigned long value) {
	unsigned char groups[10];
	unsigned int length = 0;
	do {
		groups[length++] = value & 0x7F;
		value >>= 7;
	} while (value != 0);
	while (1 < length) {
		_writeByte(buffer, groups[--length] | 0x80);
	}
	_writeByte(buffer, groups[0]);
}

/* MIDI EVENTS */

static void _writeDelta(MidiTrack * track, const unsigned long tick) {
	_writeVariableLength(&track->buffer, tick - track->lastTick);
	track->lastTick = tick;
}

static void _writeMetaEvent(MidiTrack * track, const unsigned long tick, const unsigned char type, const void * data, const size_t length) {
	_writeDelta(track, tick);
	_writeByte(&track->buffer, 0xFF);
	_writeByte(&track->buffer, type);
	_writeVariableLength(&track->buffer, length);
	_writeBytes(&track->buffer, data, length);
}

static void _writeTextEvent(MidiTrack * track, const unsigned char type, const char * text) {
	_writeMetaEvent(track, 0, type, text, strlen(text));
}

static void _writeChannelEvent(MidiTrack * track, const unsigned long tick, const unsigned char status, const unsigned char data1, const int data2) {
	_writeDelta(track, tick);
	_writeByte(&track->buffer, status | track->channel);
	_writeByte(&track->buffer, data1);
	if (0 <= data2) {
		_writeByte(&track->buffer, (unsigned char) data2);
	}
}

static void _addNoteEvent(MidiTrack * track, const unsigned long tick, const unsigned char status, const unsigned char note, const unsigned char velocity) {
	if (track->eventCount == track->eventCapacity) {
		track->eventCapacity = track->eventCapacity == 0 ? 256 : 2 * track->eventCapacity;
		track->events = realloc(track->events, track->eventCapacity * sizeof(NoteEvent));
	}
	track->events[track->eventCount++] = (NoteEvent) { tick, status, note, velocity };
}

/** By time; at the same tick, note-offs (0x80) go before note-ons (0x90). */
static int _compareNoteEvents(const void * a, const void * b) {
	const NoteEvent * x = a;
	const NoteEvent * y = b;
	if (x->tick != y->tick) {
		return x->tick < y->tick ? -1 : 1;
	}
	return (int) x->status - (int) y->status;
}

static void _flushNoteEvents(MidiTrack * track) {
	qsort(track->events, track->eventCount, sizeof(NoteEvent), _compareNoteEvents);
	for (size_t k = 0; k < track->eventCount; ++k) {
		const NoteEvent * event = &track->events[k];
		_writeChannelEvent(track, event->tick, event->status, event->note, event->velocity);
	}
}

static void _writeEndOfTrack(MidiTrack * track, const unsigned long tick) {
	_writeMetaEvent(track, tick < track->lastTick ? track->lastTick : tick, 0x2F, NULL, 0);
}

/* SONG SETTINGS AND INSTRUMENTS */

static SongSettings _readSettings(const Song * song) {
	SongSettings settings = {
		.name = song->name,
		.tempo = DEFAULT_TEMPO,
		.numerator = DEFAULT_NUMERATOR,
		.denominator = DEFAULT_DENOMINATOR
	};
	for (SongElement * element = song->firstElement; element != NULL; element = element->next) {
		switch (element->type) {
			case AUTHOR_ELEMENT:
				settings.author = element->text;
				break;
			case COPYRIGHT_ELEMENT:
				settings.copyright = element->text;
				break;
			case KEY_ELEMENT:
				settings.key = element->key;
				break;
			case TEMPO_ELEMENT:
				settings.tempo = element->tempo;
				break;
			case TIME_SIGNATURE_ELEMENT:
				settings.numerator = element->timeSignature->numerator;
				settings.denominator = element->timeSignature->denominator;
				break;
			default:
				break;
		}
	}
	return settings;
}

/**
 * The tempo of B# is expressed in beats per minute, where the beat is the
 * denominator of the time signature (e.g., an eighth note in 6/8). MIDI
 * needs microseconds per quarter note.
 */
static unsigned long _microsecondsPerQuarter(const SongSettings * settings) {
	return (60000000UL * (unsigned long) settings->denominator) / (4UL * (unsigned long) settings->tempo);
}

/**
 * The instrument of a track. An unknown name falls back to the default
 * instrument (piano), with a warning.
 */
static const Instrument * _instrumentOf(const char * name) {
	const Instrument * instrument = findInstrument(name);
	if (instrument == NULL) {
		logWarning(_logger, "Unknown instrument '%s', it will be played with '%s'.", name, defaultInstrument()->name);
		return defaultInstrument();
	}
	return instrument;
}

/**
 * The MIDI track of an instrument, created on its first appearance (one
 * channel each, skipping the percussion channel). If there are no channels
 * left, the instrument shares the last track.
 */
static MidiTrack * _trackOf(GenerationContext * context, const Instrument * instrument) {
	for (unsigned int k = 0; k < context->trackCount; ++k) {
		if (context->tracks[k].instrument == instrument) {
			return &context->tracks[k];
		}
	}
	if (context->trackCount == MAX_INSTRUMENTS) {
		logWarning(_logger, "Too many instruments (the maximum is %d), '%s' shares the track of '%s'.",
			MAX_INSTRUMENTS, instrument->name, context->tracks[MAX_INSTRUMENTS - 1].instrument->name);
		return &context->tracks[MAX_INSTRUMENTS - 1];
	}
	const unsigned int index = context->trackCount++;
	MidiTrack * track = &context->tracks[index];
	*track = (MidiTrack) {
		.instrument = instrument,
		.channel = index < PERCUSSION_CHANNEL ? index : index + 1
	};
	_writeTextEvent(track, 0x03, instrument->name);
	_writeChannelEvent(track, 0, 0xC0, instrument->program, -1);
	return track;
}

/* CHORDS AND BARS */

/**
 * Builds the MIDI notes of a chord for an instrument. Bass instruments only
 * play the bass note (the slash bass, or the root). Harmonic instruments play
 * every chord tone from C3 upwards, plus the slash bass an octave below. An
 * unknown quality is played as a major triad, with a warning.
 */
static unsigned int _chordNotes(const Chord * chord, const Instrument * instrument, unsigned char notes[MAX_CHORD_INTERVALS + 1]) {
	const int root = pitchClassOf(chord->root);
	const int bass = chord->bass == NULL ? root : pitchClassOf(chord->bass);
	if (instrument->bassLine) {
		notes[0] = BASS_LINE_BASE_NOTE + (bass - 4 + 12) % 12;
		return 1;
	}
	const ChordQuality * quality = findChordQuality(chord->quality);
	if (quality == NULL) {
		logWarning(_logger, "Unknown chord quality '%s' in '%s', it will be played as a major triad.", chord->quality, chord->symbol);
		quality = findChordQuality("");
	}
	unsigned int length = 0;
	if (chord->bass != NULL) {
		notes[length++] = SLASH_BASS_BASE_NOTE + bass;
	}
	for (unsigned int k = 0; k < quality->length; ++k) {
		notes[length++] = CHORD_BASE_NOTE + root + quality->intervals[k];
	}
	return length;
}

/**
 * The duration (in beats) of every chord of a bar. Chords with an explicit
 * duration "C(2)" keep it. The chords without duration share the remaining
 * beats of the bar equally (a single chord fills the entire bar), with at
 * least 1 beat each.
 */
static void _resolveDurations(const GenerationContext * context, const Bar * bar, int * durations) {
	int explicitBeats = 0;
	int implicitChords = 0;
	for (ChordEntry * entry = bar->firstEntry; entry != NULL; entry = entry->next) {
		if (entry->hasDuration) {
			explicitBeats += entry->duration;
		}
		else {
			++implicitChords;
		}
	}
	const int remaining = context->settings.numerator - explicitBeats;
	const int shared = implicitChords == 0 || remaining < implicitChords ? 1 : remaining / implicitChords;
	unsigned int k = 0;
	for (ChordEntry * entry = bar->firstEntry; entry != NULL; entry = entry->next) {
		durations[k++] = entry->hasDuration ? entry->duration : shared;
	}
}

static void _generateBar(GenerationContext * context, MidiTrack * track, const Bar * bar, unsigned long * tick) {
	int * durations = calloc(bar->entries, sizeof(int));
	_resolveDurations(context, bar, durations);
	unsigned int k = 0;
	for (ChordEntry * entry = bar->firstEntry; entry != NULL; entry = entry->next) {
		const unsigned long duration = (unsigned long) durations[k++] * context->beatTicks;
		if (entry->chord->type == CHORD_SYMBOL) {
			unsigned char notes[MAX_CHORD_INTERVALS + 1];
			const unsigned int length = _chordNotes(entry->chord, track->instrument, notes);
			const int velocity = track->instrument->bassLine ? BASS_VELOCITY : CHORD_VELOCITY;
			for (unsigned int n = 0; n < length; ++n) {
				_addNoteEvent(track, *tick, 0x90, notes[n], velocity);
			}
			for (unsigned int n = 0; n < length; ++n) {
				_addNoteEvent(track, *tick + duration, 0x80, notes[n], 0);
			}
		}
		// "N.C." is silence: only the time advances.
		*tick += duration;
	}
	free(durations);
}

/**
 * Writes a sequence of bars, expanding the pattern usages with the symbol
 * table. Returns the amount of bars written.
 */
static unsigned long _generateContent(GenerationContext * context, MidiTrack * track, const Content * content, unsigned long * tick, const unsigned int depth) {
	unsigned long bars = 0;
	if (MAX_PATTERN_DEPTH < depth) {
		if (!context->failed) {
			logError(_logger, "The patterns are nested more than %d levels (is there a pattern that uses itself?).", MAX_PATTERN_DEPTH);
		}
		context->failed = true;
		return 0;
	}
	for (ContentElement * element = content->first; element != NULL && !context->failed; element = element->next) {
		if (element->type == BAR_ELEMENT) {
			_generateBar(context, track, element->bar, tick);
			++bars;
		}
		else {
			const PatternUsage * usage = element->patternUsage;
			const Symbol * symbol = findSymbol(context->symbolTable, usage->name, SYMBOL_PATTERN);
			if (symbol == NULL) {
				logError(_logger, "The pattern '%s' is not defined.", usage->name);
				context->failed = true;
				return bars;
			}
			const Pattern * pattern = symbol->definition;
			const int repetitions = usage->hasRepetitions ? usage->repetitions : 1;
			for (int k = 0; k < repetitions && !context->failed; ++k) {
				bars += _generateContent(context, track, pattern->content, tick, depth + 1);
			}
		}
	}
	return bars;
}

/**
 * Writes a section (every repetition) starting at "tick", and returns the
 * tick where the next section starts. Parallel tracks start at the same tick,
 * and the section lasts as long as its longest track.
 */
static unsigned long _generateSection(GenerationContext * context, const Section * section, unsigned long tick) {
	const int repetitions = section->hasRepetitions ? section->repetitions : 1;
	for (int k = 0; k < repetitions && !context->failed; ++k) {
		unsigned long end = tick;
		unsigned long bars = 0;
		if (section->bodyType == CONTENT_BODY) {
			bars = _generateContent(context, _trackOf(context, defaultInstrument()), section->content, &end, 0);
		}
		else {
			for (Track * track = section->trackList->first; track != NULL; track = track->next) {
				unsigned long trackEnd = tick;
				const unsigned long trackBars = _generateContent(context, _trackOf(context, _instrumentOf(track->instrument)), track->content, &trackEnd, 0);
				if (end < trackEnd) {
					end = trackEnd;
				}
				if (bars < trackBars) {
					bars = trackBars;
				}
			}
		}
		context->bars += bars;
		tick = end;
	}
	return tick;
}

/* CONDUCTOR TRACK */

static unsigned char _log2(int value) {
	unsigned char result = 0;
	while (1 < value) {
		value >>= 1;
		++result;
	}
	return result;
}

static void _generateConductorTrack(const GenerationContext * context, MidiTrack * conductor, const unsigned long songTicks) {
	const SongSettings * settings = &context->settings;
	_writeTextEvent(conductor, 0x03, settings->name);
	if (settings->author != NULL) {
		char * text = calloc(strlen(settings->author) + 16, sizeof(char));
		sprintf(text, "Author: %s", settings->author);
		_writeTextEvent(conductor, 0x01, text);
		free(text);
	}
	if (settings->copyright != NULL) {
		_writeTextEvent(conductor, 0x02, settings->copyright);
	}
	const unsigned long microseconds = _microsecondsPerQuarter(settings);
	const unsigned char tempo[] = { (microseconds >> 16) & 0xFF, (microseconds >> 8) & 0xFF, microseconds & 0xFF };
	_writeMetaEvent(conductor, 0, 0x51, tempo, sizeof(tempo));
	const unsigned char timeSignature[] = { settings->numerator, _log2(settings->denominator), 24, 8 };
	_writeMetaEvent(conductor, 0, 0x58, timeSignature, sizeof(timeSignature));
	int accidentals = 0;
	if (settings->key != NULL) {
		if (keySignatureOf(settings->key->note, settings->key->mode, &accidentals)) {
			const unsigned char keySignature[] = { (unsigned char) (signed char) accidentals, settings->key->mode == MINOR_MODE };
			_writeMetaEvent(conductor, 0, 0x59, keySignature, sizeof(keySignature));
		}
		else {
			logWarning(_logger, "The key '%s %s' is theoretical (more than 7 accidentals), the MIDI file won't include its key signature.",
				settings->key->note, settings->key->mode == MAJOR_MODE ? "major" : "minor");
		}
	}
	_writeEndOfTrack(conductor, songTicks);
}

/**
 * Checks the values that MIDI cannot represent. The rest of the validations
 * belong to the semantic analysis.
 */
static bool _isRepresentable(const SongSettings * settings) {
	if (settings->tempo <= 0) {
		logError(_logger, "Invalid tempo %d BPM: it must be a positive number.", settings->tempo);
		return false;
	}
	if (settings->numerator < 1 || 255 < settings->numerator || settings->denominator < 1 || 4 * PPQ < settings->denominator) {
		logError(_logger, "The time signature %d/%d cannot be represented in MIDI.", settings->numerator, settings->denominator);
		return false;
	}
	if (MAX_MIDI_TEMPO < _microsecondsPerQuarter(settings)) {
		logError(_logger, "The tempo %d BPM in %d/%d is too slow to be represented in MIDI.", settings->tempo, settings->numerator, settings->denominator);
		return false;
	}
	return true;
}

/* FILE */

static void _appendTrack(ByteBuffer * file, const ByteBuffer * track) {
	_writeBytes(file, "MTrk", 4);
	_writeInteger(file, track->length, 4);
	_writeBytes(file, track->bytes, track->length);
}

static CompilationStatus _writeFile(const char * path, const ByteBuffer * file) {
	FILE * output = fopen(path, "wb");
	CompilationStatus status = SUCCEEDED;
	if (output == NULL || fwrite(file->bytes, 1, file->length, output) != file->length) {
		logError(_logger, "Cannot write the MIDI file: \"%s\".", path);
		status = FAILED;
	}
	if (output != NULL) {
		fclose(output);
	}
	return status;
}

/* PUBLIC FUNCTIONS */

CompilationStatus executeGenerator(CompilerState * compilerState) {
	Program * program = compilerState->abstractSyntaxtTree;
	GenerationContext context = {
		.settings = _readSettings(program->song),
		.symbolTable = compilerState->symbolTable
	};
	if (!_isRepresentable(&context.settings)) {
		return FAILED;
	}
	context.beatTicks = (PPQ * 4) / context.settings.denominator;

	unsigned long tick = 0;
	for (SongElement * element = program->song->firstElement; element != NULL && !context.failed; element = element->next) {
		if (element->type == SECTION_ELEMENT) {
			tick = _generateSection(&context, element->section, tick);
		}
	}

	CompilationStatus status = FAILED;
	MidiTrack conductor = { .channel = 0 };
	ByteBuffer file = { 0 };
	if (!context.failed) {
		_generateConductorTrack(&context, &conductor, tick);
		for (unsigned int k = 0; k < context.trackCount; ++k) {
			_flushNoteEvents(&context.tracks[k]);
			_writeEndOfTrack(&context.tracks[k], tick);
		}
		_writeBytes(&file, "MThd", 4);
		_writeInteger(&file, 6, 4);
		_writeInteger(&file, 1, 2);
		_writeInteger(&file, 1 + context.trackCount, 2);
		_writeInteger(&file, PPQ, 2);
		_appendTrack(&file, &conductor.buffer);
		for (unsigned int k = 0; k < context.trackCount; ++k) {
			_appendTrack(&file, &context.tracks[k].buffer);
		}
		status = _writeFile(compilerState->outputPath, &file);
		if (status == SUCCEEDED) {
			const double seconds = (double) tick / PPQ * 60.0 / context.settings.tempo * (context.settings.denominator / 4.0);
			logInformation(_logger, "MIDI file generated: \"%s\" (%u instrument track(s), %lu bars, %.1f seconds).",
				compilerState->outputPath, context.trackCount, context.bars, seconds);
		}
	}
	free(conductor.buffer.bytes);
	for (unsigned int k = 0; k < context.trackCount; ++k) {
		free(context.tracks[k].buffer.bytes);
		free(context.tracks[k].events);
	}
	free(file.bytes);
	return status;
}
