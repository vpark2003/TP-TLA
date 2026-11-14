#include "SemanticAnalyzer.h"
#include "SymbolTable.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ESTADO INTERNO DEL MÓDULO */

#define CONTEXT_SIZE 256
#define MESSAGE_SIZE 2048

static Logger * _logger = NULL;

static void _shutdownSemanticAnalyzerModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: SemanticAnalyzer...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeSemanticAnalyzerModule() {
	_logger = createLogger("SemanticAnalyzer");
	return _shutdownSemanticAnalyzerModule;
}

/* Estado de un patrón durante la búsqueda en profundidad (detecta ciclos). */
typedef enum {
	UNVISITED,
	VISITING,
	VISITED
} PatternState;

typedef struct {
	Pattern * pattern;
	PatternState state;
	unsigned long long bars;
	unsigned int usages;
} PatternInfo;

/* Todo lo que se necesita durante una ejecución del análisis. */
typedef struct {
	SymbolTable * table;
	unsigned int errors;
	unsigned int warnings;
	int numerator;
	int denominator;
	Key * key;
	PatternInfo * patterns;
	unsigned int patternCount;
	const Instrument * instruments[MAX_INSTRUMENTS];
	unsigned int instrumentCount;
	unsigned long long totalBars;
} Analysis;

/* MENSAJES */

static void _error(Analysis * analysis, const char * const format, ...) {
	char message[MESSAGE_SIZE];
	va_list arguments;
	va_start(arguments, format);
	vsnprintf(message, sizeof(message), format, arguments);
	va_end(arguments);
	logError(_logger, "%s", message);
	++analysis->errors;
}

static void _warning(Analysis * analysis, const char * const format, ...) {
	char message[MESSAGE_SIZE];
	va_list arguments;
	va_start(arguments, format);
	vsnprintf(message, sizeof(message), format, arguments);
	va_end(arguments);
	logWarning(_logger, "%s", message);
	++analysis->warnings;
}

/* Satura la cantidad de compases para que las repeticiones no desborden. */
static unsigned long long _saturate(const unsigned long long bars) {
	return bars > MAX_SONG_BARS ? MAX_SONG_BARS + 1 : bars;
}

/* REPETICIONES Y ACORDES */

/*
 * Las repeticiones "xN" deben ser un natural entre 1 y MAX_REPETITIONS. Sin
 * "xN", el bloque se ejecuta una vez.
 */
static unsigned int _validateRepetitions(Analysis * analysis, const bool hasRepetitions, const int repetitions, const char * context) {
	if (!hasRepetitions) {
		return 1;
	}
	if (repetitions < 1 || MAX_REPETITIONS < repetitions) {
		_error(analysis, "%s: invalid repetitions 'x%d' (must be between x1 and x%d).", context, repetitions, MAX_REPETITIONS);
		return 1;
	}
	return (unsigned int) repetitions;
}

/* La cualidad debe ser conocida; un acorde fuera de la tonalidad es una advertencia. */
static void _validateChord(Analysis * analysis, const Chord * chord, const char * context) {
	if (chord->type == NO_CHORD_SYMBOL) {
		return;
	}
	if (findChordQuality(chord->quality) == NULL) {
		_error(analysis, "%s: unknown chord quality '%s' in chord '%s'.", context, chord->quality, chord->symbol);
		return;
	}
	if (analysis->key != NULL && !isDiatonicChord(analysis->key->note, analysis->key->mode, chord)) {
		_warning(analysis, "%s: chord '%s' is not diatonic to the key of %s.", context, chord->symbol, analysis->key->note);
	}
}

/* COMPASES */

static void _validateChordEntries(Analysis * analysis, ChordEntry * entry, const char * context, int * explicitBeats, int * implicitChords) {
	if (entry == NULL) {
		return;
	}
	_validateChord(analysis, entry->chord, context);
	if (entry->hasDuration) {
		if (entry->duration <= 0) {
			_error(analysis, "%s: chord '%s' has an invalid duration of %d beats (must be at least 1).",
				context, entry->chord->type == NO_CHORD_SYMBOL ? "N.C." : entry->chord->symbol, entry->duration);
		}
		*explicitBeats += entry->duration;
	}
	else {
		++*implicitChords;
	}
	_validateChordEntries(analysis, entry->next, context, explicitBeats, implicitChords);
}

/*
 * Los tiempos del compás deben sumar exactamente el numerador del compás. Los
 * acordes sin duración se reparten en partes iguales los tiempos restantes
 * (un único acorde ocupa todo el compás); si no se pueden repartir de forma
 * exacta, es un error.
 */
static void _validateBar(Analysis * analysis, Bar * bar, const char * context) {
	const unsigned int errors = analysis->errors;
	int explicitBeats = 0;
	int implicitChords = 0;
	_validateChordEntries(analysis, bar->firstEntry, context, &explicitBeats, &implicitChords);
	if (errors != analysis->errors) {
		return;
	}
	const int beats = analysis->numerator;
	if (implicitChords == 0) {
		if (explicitBeats != beats) {
			_error(analysis, "%s: the durations sum %d beats, but the time signature %d/%d requires %d.",
				context, explicitBeats, analysis->numerator, analysis->denominator, beats);
		}
		return;
	}
	const int remaining = beats - explicitBeats;
	if (remaining < implicitChords || remaining % implicitChords != 0) {
		_error(analysis, "%s: cannot split the remaining %d beats evenly among the %d chords without duration (time signature %d/%d).",
			context, remaining, implicitChords, analysis->numerator, analysis->denominator);
	}
}

/* PATRONES Y CONTENIDO */

static PatternInfo * _findPatternInfo(Analysis * analysis, const char * name) {
	Symbol * symbol = findSymbol(analysis->table, name, SYMBOL_PATTERN);
	if (symbol == NULL) {
		return NULL;
	}
	for (unsigned int k = 0; k < analysis->patternCount; ++k) {
		if (analysis->patterns[k].pattern == symbol->definition) {
			return &analysis->patterns[k];
		}
	}
	return NULL;
}

static unsigned long long _resolvePattern(Analysis * analysis, PatternInfo * info);

/*
 * Valida una secuencia de compases y usos de patrones, y devuelve su largo en
 * compases (expandiendo los patrones y sus repeticiones).
 */
static unsigned long long _validateContent(Analysis * analysis, Content * content, const char * owner) {
	unsigned long long bars = 0;
	unsigned int barNumber = 0;
	char context[4 * CONTEXT_SIZE];
	for (ContentElement * element = content->first; element != NULL; element = element->next) {
		if (element->type == BAR_ELEMENT) {
			snprintf(context, sizeof(context), "%s, bar %u", owner, ++barNumber);
			_validateBar(analysis, element->bar, context);
			bars = _saturate(bars + 1);
		}
		else {
			PatternUsage * usage = element->patternUsage;
			snprintf(context, sizeof(context), "%s, 'use %s'", owner, usage->name);
			const unsigned int repetitions = _validateRepetitions(analysis, usage->hasRepetitions, usage->repetitions, context);
			PatternInfo * info = _findPatternInfo(analysis, usage->name);
			if (info == NULL) {
				_error(analysis, "%s: the pattern '%s' is not defined.", context, usage->name);
				continue;
			}
			++info->usages;
			bars = _saturate(bars + _saturate(repetitions * _resolvePattern(analysis, info)));
		}
	}
	return bars;
}

/* Calcula el largo de un patrón una sola vez, detectando usos cíclicos. */
static unsigned long long _resolvePattern(Analysis * analysis, PatternInfo * info) {
	if (info->state == VISITED) {
		return info->bars;
	}
	if (info->state == VISITING) {
		_error(analysis, "The pattern '%s' uses itself (directly or through other patterns).", info->pattern->name);
		return 0;
	}
	info->state = VISITING;
	char owner[CONTEXT_SIZE];
	snprintf(owner, sizeof(owner), "Pattern '%s'", info->pattern->name);
	info->bars = _validateContent(analysis, info->pattern->content, owner);
	info->state = VISITED;
	return info->bars;
}

/* SECCIONES Y PISTAS */

static void _registerInstrument(Analysis * analysis, const Instrument * instrument, const char * context) {
	for (unsigned int k = 0; k < analysis->instrumentCount; ++k) {
		if (analysis->instruments[k] == instrument) {
			return;
		}
	}
	if (analysis->instrumentCount == MAX_INSTRUMENTS) {
		_error(analysis, "%s: too many different instruments (the maximum is %d), '%s' cannot be added.", context, MAX_INSTRUMENTS, instrument->name);
		return;
	}
	analysis->instruments[analysis->instrumentCount++] = instrument;
}

/*
 * Cada pista debe usar un instrumento conocido, que no se repita dentro de la
 * sección, y todas las pistas deben tener la misma cantidad de compases.
 */
static unsigned long long _validateTracks(Analysis * analysis, Section * section, const char * owner) {
	char context[2 * CONTEXT_SIZE];
	char instruments[MESSAGE_SIZE];
	unsigned long long firstTrackBars = 0;
	for (Track * track = section->trackList->first; track != NULL; track = track->next) {
		snprintf(context, sizeof(context), "%s, track '%s'", owner, track->instrument);
		const Instrument * instrument = findInstrument(track->instrument);
		if (instrument == NULL) {
			listInstruments(instruments, sizeof(instruments));
			_error(analysis, "%s: unknown instrument '%s'. Available instruments: %s.", context, track->instrument, instruments);
		}
		else {
			for (Track * previous = section->trackList->first; previous != track; previous = previous->next) {
				if (strcmp(previous->instrument, track->instrument) == 0) {
					_error(analysis, "%s: the instrument '%s' is used twice in the same section.", context, track->instrument);
					break;
				}
			}
			_registerInstrument(analysis, instrument, context);
		}
		const unsigned long long trackBars = _validateContent(analysis, track->content, context);
		if (track == section->trackList->first) {
			firstTrackBars = trackBars;
		}
		else if (trackBars != firstTrackBars) {
			_error(analysis, "%s: parallel tracks must have the same length (this one has %llu bars, but track '%s' has %llu).",
				context, trackBars, section->trackList->first->instrument, firstTrackBars);
		}
	}
	return firstTrackBars;
}

static void _validateSection(Analysis * analysis, Section * section) {
	char owner[CONTEXT_SIZE];
	snprintf(owner, sizeof(owner), "Section '%s'", section->name);
	const unsigned int repetitions = _validateRepetitions(analysis, section->hasRepetitions, section->repetitions, owner);
	unsigned long long bars = 0;
	if (section->bodyType == CONTENT_BODY) {
		_registerInstrument(analysis, defaultInstrument(), owner);
		bars = _validateContent(analysis, section->content, owner);
	}
	else {
		bars = _validateTracks(analysis, section, owner);
	}
	analysis->totalBars = _saturate(analysis->totalBars + _saturate(repetitions * bars));
}

/* CONFIGURACIÓN GLOBAL */

static bool _isPowerOfTwo(int value) {
	if (value <= 0) {
		return false;
	}
	while (value % 2 == 0) {
		value = value / 2;
	}
	return value == 1;
}

static void _validateTimeSignature(Analysis * analysis, TimeSignature * timeSignature) {
	if (timeSignature->numerator < 1 || MAX_NUMERATOR < timeSignature->numerator) {
		_error(analysis, "Invalid time signature %d/%d: the numerator must be between 1 and %d.",
			timeSignature->numerator, timeSignature->denominator, MAX_NUMERATOR);
	}
	if (!_isPowerOfTwo(timeSignature->denominator) || MAX_DENOMINATOR < timeSignature->denominator) {
		_error(analysis, "Invalid time signature %d/%d: the denominator must be 1, 2, 4, 8, 16 or 32.",
			timeSignature->numerator, timeSignature->denominator);
	}
	analysis->numerator = timeSignature->numerator;
	analysis->denominator = timeSignature->denominator;
}

static void _validateKey(Analysis * analysis, Key * key) {
	int accidentals = 0;
	if (!keySignatureOf(key->note, key->mode, &accidentals)) {
		_error(analysis, "The key '%s %s' is theoretical (it needs more than 7 accidentals), use its enharmonic equivalent instead.",
			key->note, key->mode == MAJOR_MODE ? "major" : "minor");
	}
	analysis->key = key;
}

/*
 * PRIMERA PASADA: la configuración global (en cualquier orden, a lo sumo una
 * vez cada una) y el registro de patrones y secciones en la tabla de símbolos.
 */
static void _registerSongElements(Analysis * analysis, Song * song) {
	bool seen[TIME_SIGNATURE_ELEMENT + 1] = { false };
	static const char * keywords[TIME_SIGNATURE_ELEMENT + 1] = {
		[AUTHOR_ELEMENT] = "author",
		[COPYRIGHT_ELEMENT] = "copyright",
		[KEY_ELEMENT] = "key",
		[TEMPO_ELEMENT] = "tempo",
		[TIME_SIGNATURE_ELEMENT] = "time"
	};
	for (SongElement * element = song->firstElement; element != NULL; element = element->next) {
		if (keywords[element->type] != NULL) {
			if (seen[element->type]) {
				_error(analysis, "The song declares '%s' more than once.", keywords[element->type]);
			}
			seen[element->type] = true;
		}
		switch (element->type) {
			case KEY_ELEMENT:
				_validateKey(analysis, element->key);
				break;
			case TEMPO_ELEMENT:
				if (element->tempo < MIN_TEMPO || MAX_TEMPO < element->tempo) {
					_error(analysis, "Invalid tempo %d BPM (must be between %d and %d).", element->tempo, MIN_TEMPO, MAX_TEMPO);
				}
				break;
			case TIME_SIGNATURE_ELEMENT:
				_validateTimeSignature(analysis, element->timeSignature);
				break;
			case PATTERN_ELEMENT:
				if (!insertSymbol(analysis->table, element->pattern->name, SYMBOL_PATTERN, element->pattern)) {
					_error(analysis, "The pattern '%s' is defined more than once.", element->pattern->name);
					break;
				}
				analysis->patterns = realloc(analysis->patterns, (analysis->patternCount + 1) * sizeof(PatternInfo));
				analysis->patterns[analysis->patternCount++] = (PatternInfo) {
					.pattern = element->pattern,
					.state = UNVISITED
				};
				break;
			case SECTION_ELEMENT:
				// Una canción puede repetir una sección (verse, chorus, verse): solo
				// se registra la primera aparición de cada nombre.
				insertSymbol(analysis->table, element->section->name, SYMBOL_SECTION, element->section);
				break;
			default:
				break;
		}
	}
}

/*
 * SEGUNDA PASADA: patrones (aunque no se usen), secciones, y las advertencias
 * y límites globales.
 */
static void _validateSongElements(Analysis * analysis, Song * song) {
	for (unsigned int k = 0; k < analysis->patternCount; ++k) {
		_resolvePattern(analysis, &analysis->patterns[k]);
	}
	unsigned int sections = 0;
	for (SongElement * element = song->firstElement; element != NULL; element = element->next) {
		if (element->type == SECTION_ELEMENT) {
			_validateSection(analysis, element->section);
			++sections;
		}
	}
	for (unsigned int k = 0; k < analysis->patternCount; ++k) {
		if (analysis->patterns[k].usages == 0) {
			_warning(analysis, "The pattern '%s' is defined but never used.", analysis->patterns[k].pattern->name);
		}
	}
	if (sections == 0) {
		_warning(analysis, "The song '%s' has no sections, the MIDI file will be silent.", song->name);
	}
	if (MAX_SONG_BARS < analysis->totalBars) {
		_error(analysis, "The song is too long (more than %d bars after expanding repetitions and patterns).", MAX_SONG_BARS);
	}
}

/* FUNCIÓN PRINCIPAL DEL ANÁLISIS SEMÁNTICO */

bool executeSemanticAnalysis(CompilerState * compilerState) {
	if (compilerState == NULL || compilerState->abstractSyntaxtTree == NULL) {
		return false;
	}
	Program * program = compilerState->abstractSyntaxtTree;
	if (program->song == NULL) {
		return false;
	}
	compilerState->symbolTable = createSymbolTable();
	if (compilerState->symbolTable == NULL) {
		return false;
	}
	Analysis analysis = {
		.table = compilerState->symbolTable,
		.numerator = 4,
		.denominator = 4
	};
	_registerSongElements(&analysis, program->song);
	// Sin una configuración válida (sobre todo el compás), no tiene sentido
	// validar los compases.
	if (analysis.errors == 0) {
		_validateSongElements(&analysis, program->song);
	}
	free(analysis.patterns);
	if (analysis.errors == 0) {
		logDebugging(_logger, "The semantic analysis accepts the program (%u warning(s), %llu bars).", analysis.warnings, analysis.totalBars);
		return true;
	}
	logError(_logger, "The semantic analysis rejects the program with %u error(s).", analysis.errors);
	return false;
}
