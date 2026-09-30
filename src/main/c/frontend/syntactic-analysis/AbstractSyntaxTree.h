#ifndef ABSTRACT_SYNTAX_TREE_HEADER
#define ABSTRACT_SYNTAX_TREE_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdbool.h>
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeAbstractSyntaxTreeModule();

/**
 * This type definitions allows self-referencing types (e.g., a list of bars
 * where each bar points to the next one).
 */

typedef enum ChordType ChordType;
typedef enum ContentElementType ContentElementType;
typedef enum Mode Mode;
typedef enum SectionBodyType SectionBodyType;
typedef enum SongElementType SongElementType;

typedef struct Bar Bar;
typedef struct Chord Chord;
typedef struct ChordEntry ChordEntry;
typedef struct Content Content;
typedef struct ContentElement ContentElement;
typedef struct Key Key;
typedef struct Pattern Pattern;
typedef struct PatternUsage PatternUsage;
typedef struct Program Program;
typedef struct Section Section;
typedef struct Song Song;
typedef struct SongElement SongElement;
typedef struct TimeSignature TimeSignature;
typedef struct Track Track;
typedef struct TrackList TrackList;

/**
 * Node types for the Abstract Syntax Tree (AST).
 *
 * Lists are simply-linked (every node has a "next" pointer), and the list
 * holders keep a pointer to the last node so appending is O(1).
 */

enum ChordType {
	CHORD_SYMBOL,
	NO_CHORD_SYMBOL
};

enum ContentElementType {
	BAR_ELEMENT,
	PATTERN_USAGE_ELEMENT
};

enum Mode {
	MAJOR_MODE,
	MINOR_MODE
};

enum SectionBodyType {
	CONTENT_BODY,
	TRACKS_BODY
};

enum SongElementType {
	AUTHOR_ELEMENT,
	COPYRIGHT_ELEMENT,
	KEY_ELEMENT,
	PATTERN_ELEMENT,
	SECTION_ELEMENT,
	TEMPO_ELEMENT,
	TIME_SIGNATURE_ELEMENT
};

/**
 * A chord symbol, like "Cmaj7/E": root="C", quality="maj7", bass="E". The
 * musical interpretation of the quality is left to the semantic analysis.
 * For "N.C." (no chord), only the type is relevant.
 */
struct Chord {
	ChordType type;
	char * symbol;
	char * root;
	char * quality;
	char * bass;
};

/**
 * A chord inside a bar, with an optional duration in beats: "C(2)".
 */
struct ChordEntry {
	Chord * chord;
	bool hasDuration;
	int duration;
	ChordEntry * next;
};

/**
 * A bar (compás): "| C(2) G(2) |".
 */
struct Bar {
	ChordEntry * firstEntry;
	ChordEntry * lastEntry;
	unsigned int entries;
};

/**
 * A reference to a pattern: "use intro x2".
 */
struct PatternUsage {
	char * name;
	bool hasRepetitions;
	int repetitions;
};

struct ContentElement {
	union {
		Bar * bar;
		PatternUsage * patternUsage;
	};
	ContentElementType type;
	ContentElement * next;
};

/**
 * A sequence of bars and pattern usages, in order.
 */
struct Content {
	ContentElement * first;
	ContentElement * last;
	unsigned int elements;
};

/**
 * An instrument track inside a section: "track piano { ... }". Every track
 * of the same section is meant to be played in parallel.
 */
struct Track {
	char * instrument;
	Content * content;
	Track * next;
};

struct TrackList {
	Track * first;
	Track * last;
	unsigned int tracks;
};

/**
 * A section: "section chorus x2 { ... }". It contains either bars and
 * pattern usages directly, or a list of parallel tracks.
 */
struct Section {
	char * name;
	bool hasRepetitions;
	int repetitions;
	union {
		Content * content;
		TrackList * trackList;
	};
	SectionBodyType bodyType;
};

/**
 * A reusable fragment: "pattern intro { | C | Am | }".
 */
struct Pattern {
	char * name;
	Content * content;
};

struct TimeSignature {
	int numerator;
	int denominator;
};

struct Key {
	char * note;
	Mode mode;
};

struct SongElement {
	union {
		char * text;
		int tempo;
		Key * key;
		Pattern * pattern;
		Section * section;
		TimeSignature * timeSignature;
	};
	SongElementType type;
	SongElement * next;
};

struct Song {
	char * name;
	SongElement * firstElement;
	SongElement * lastElement;
	unsigned int elements;
};

struct Program {
	Song * song;
};

/**
 * Node recursive super-duper-trambolik-destructors.
 */

void destroyBar(Bar * bar);
void destroyChord(Chord * chord);
void destroyChordEntry(ChordEntry * chordEntry);
void destroyContent(Content * content);
void destroyContentElement(ContentElement * contentElement);
void destroyKey(Key * key);
void destroyPattern(Pattern * pattern);
void destroyPatternUsage(PatternUsage * patternUsage);
void destroyProgram(Program * program);
void destroySection(Section * section);
void destroySong(Song * song);
void destroySongElement(SongElement * songElement);
void destroyTimeSignature(TimeSignature * timeSignature);
void destroyTrack(Track * track);
void destroyTrackList(TrackList * trackList);

#endif
