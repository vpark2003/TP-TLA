#include "BisonActions.h"

/* MODULE INTERNAL STATE */

static CompilerState * _compilerState = NULL;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownBisonActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: BisonActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_compilerState = NULL;
}

ModuleDestructor initializeBisonActionsModule(CompilerState * compilerState) {
	_compilerState = compilerState;
	_logger = createLogger("BisonActions");
	return _shutdownBisonActionsModule;
}

/* PRIVATE FUNCTIONS */

static void _logSyntacticAnalyzerAction(const char * functionName);
static SongElement * _createSongElement(const SongElementType type);

/**
 * Logs a syntactic-analyzer action in DEBUGGING level.
 */
static void _logSyntacticAnalyzerAction(const char * functionName) {
	logDebugging(_logger, "%s", functionName);
}

static SongElement * _createSongElement(const SongElementType type) {
	SongElement * songElement = calloc(1, sizeof(SongElement));
	songElement->type = type;
	songElement->next = NULL;
	return songElement;
}

/**
 * Appends a single element to a content (creating the content if NULL).
 */
static Content * _appendContentElement(Content * content, ContentElement * element) {
	if (content == NULL) {
		content = calloc(1, sizeof(Content));
	}
	if (content->last == NULL) {
		content->first = element;
	}
	else {
		content->last->next = element;
	}
	content->last = element;
	content->elements++;
	return content;
}

/* PUBLIC FUNCTIONS */

void SyntaxErrorAction(const char * message) {
	logError(_logger, "Syntax error: %s.", message);
}

Song * AppendSongElementSemanticAction(Song * song, SongElement * songElement) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (song->lastElement == NULL) {
		song->firstElement = songElement;
	}
	else {
		song->lastElement->next = songElement;
	}
	song->lastElement = songElement;
	song->elements++;
	return song;
}

TrackList * AppendTrackSemanticAction(TrackList * trackList, Track * track) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	trackList->last->next = track;
	trackList->last = track;
	trackList->tracks++;
	return trackList;
}

Content * BarContentSemanticAction(Content * content, Bar * bar) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ContentElement * element = calloc(1, sizeof(ContentElement));
	element->type = BAR_ELEMENT;
	element->bar = bar;
	return _appendContentElement(content, element);
}

Bar * BarSemanticAction(Bar * bar, ChordEntry * chordEntry) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (bar == NULL) {
		bar = calloc(1, sizeof(Bar));
		bar->firstEntry = chordEntry;
	}
	else {
		bar->lastEntry->next = chordEntry;
	}
	bar->lastEntry = chordEntry;
	bar->entries++;
	return bar;
}

ChordEntry * ChordEntrySemanticAction(Chord * chord, const bool hasDuration, const int duration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ChordEntry * chordEntry = calloc(1, sizeof(ChordEntry));
	chordEntry->chord = chord;
	chordEntry->hasDuration = hasDuration;
	chordEntry->duration = duration;
	chordEntry->next = NULL;
	return chordEntry;
}

Content * ConcatenateContentSemanticAction(Content * content, Content * tail) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	content->last->next = tail->first;
	content->last = tail->last;
	content->elements += tail->elements;
	free(tail);
	return content;
}

Section * ContentSectionSemanticAction(char * name, const int repetitions, Content * content) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Section * section = calloc(1, sizeof(Section));
	section->name = name;
	section->hasRepetitions = repetitions != NO_REPETITIONS;
	section->repetitions = section->hasRepetitions ? repetitions : 1;
	section->bodyType = CONTENT_BODY;
	section->content = content;
	return section;
}

Song * EmptySongSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return calloc(1, sizeof(Song));
}

SongElement * KeySongElementSemanticAction(char * note, const Mode mode) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Key * key = calloc(1, sizeof(Key));
	key->note = note;
	key->mode = mode;
	SongElement * songElement = _createSongElement(KEY_ELEMENT);
	songElement->key = key;
	return songElement;
}

Chord * NoChordSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Chord * chord = calloc(1, sizeof(Chord));
	chord->type = NO_CHORD_SYMBOL;
	chord->symbol = calloc(5, sizeof(char));
	strcpy(chord->symbol, "N.C.");
	return chord;
}

Pattern * PatternSemanticAction(char * name, Content * content) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Pattern * pattern = calloc(1, sizeof(Pattern));
	pattern->name = name;
	pattern->content = content;
	return pattern;
}

SongElement * PatternSongElementSemanticAction(Pattern * pattern) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	SongElement * songElement = _createSongElement(PATTERN_ELEMENT);
	songElement->pattern = pattern;
	return songElement;
}

Content * PatternUsageContentSemanticAction(Content * content, PatternUsage * patternUsage) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ContentElement * element = calloc(1, sizeof(ContentElement));
	element->type = PATTERN_USAGE_ELEMENT;
	element->patternUsage = patternUsage;
	return _appendContentElement(content, element);
}

PatternUsage * PatternUsageSemanticAction(char * name, const int repetitions) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	PatternUsage * patternUsage = calloc(1, sizeof(PatternUsage));
	patternUsage->name = name;
	patternUsage->hasRepetitions = repetitions != NO_REPETITIONS;
	patternUsage->repetitions = patternUsage->hasRepetitions ? repetitions : 1;
	return patternUsage;
}

SongElement * SectionSongElementSemanticAction(Section * section) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	SongElement * songElement = _createSongElement(SECTION_ELEMENT);
	songElement->section = section;
	return songElement;
}

Program * SongProgramSemanticAction(Song * song) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Program * program = calloc(1, sizeof(Program));
	program->song = song;
	_compilerState->abstractSyntaxtTree = program;
	return program;
}

Song * SongSemanticAction(char * name, Song * song) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	song->name = name;
	return song;
}

SongElement * TempoSongElementSemanticAction(const int tempo) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	SongElement * songElement = _createSongElement(TEMPO_ELEMENT);
	songElement->tempo = tempo;
	return songElement;
}

SongElement * TextSongElementSemanticAction(char * text, const SongElementType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	SongElement * songElement = _createSongElement(type);
	songElement->text = text;
	return songElement;
}

SongElement * TimeSignatureSongElementSemanticAction(const int numerator, const int denominator) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TimeSignature * timeSignature = calloc(1, sizeof(TimeSignature));
	timeSignature->numerator = numerator;
	timeSignature->denominator = denominator;
	SongElement * songElement = _createSongElement(TIME_SIGNATURE_ELEMENT);
	songElement->timeSignature = timeSignature;
	return songElement;
}

TrackList * TrackListSemanticAction(Track * track) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TrackList * trackList = calloc(1, sizeof(TrackList));
	trackList->first = track;
	trackList->last = track;
	trackList->tracks = 1;
	return trackList;
}

Section * TracksSectionSemanticAction(char * name, const int repetitions, TrackList * trackList) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Section * section = calloc(1, sizeof(Section));
	section->name = name;
	section->hasRepetitions = repetitions != NO_REPETITIONS;
	section->repetitions = section->hasRepetitions ? repetitions : 1;
	section->bodyType = TRACKS_BODY;
	section->trackList = trackList;
	return section;
}

Track * TrackSemanticAction(char * instrument, Content * content) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Track * track = calloc(1, sizeof(Track));
	track->instrument = instrument;
	track->content = content;
	track->next = NULL;
	return track;
}
