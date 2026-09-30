#include "Generator.h"

/* MODULE INTERNAL STATE */

const char _indentationCharacter = ' ';
const char _indentationSize = 4;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
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
 * PRIVATE FUNCTIONS
 *
 * Stage II: the "generator" prints the AST as an indented tree in the
 * standard output, so it's possible to verify that the tree was properly
 * built. In Stage III it will be replaced by the MIDI generation.
 */

static char * _indentation(const unsigned int indentationLevel);
static void _output(const unsigned int indentationLevel, const char * const format, ...);
static void _printBar(const unsigned int level, Bar * bar);
static void _printContent(const unsigned int level, Content * content);
static void _printSection(const unsigned int level, Section * section);
static void _printSong(const unsigned int level, Song * song);
static void _printSongElement(const unsigned int level, SongElement * songElement);

static void _printBar(const unsigned int level, Bar * bar) {
	_output(level, "Bar (%u chord%s)\n", bar->entries, bar->entries == 1 ? "" : "s");
	for (ChordEntry * entry = bar->firstEntry; entry != NULL; entry = entry->next) {
		Chord * chord = entry->chord;
		char duration[32] = "";
		if (entry->hasDuration) {
			snprintf(duration, sizeof(duration), ", duration=%d", entry->duration);
		}
		if (chord->type == NO_CHORD_SYMBOL) {
			_output(1 + level, "NoChord(N.C.%s)\n", duration);
		}
		else {
			_output(1 + level, "Chord(symbol=%s, root=%s, quality=\"%s\", bass=%s%s)\n",
				chord->symbol, chord->root, chord->quality,
				chord->bass == NULL ? "-" : chord->bass, duration);
		}
	}
}

static void _printContent(const unsigned int level, Content * content) {
	for (ContentElement * element = content->first; element != NULL; element = element->next) {
		switch (element->type) {
			case BAR_ELEMENT:
				_printBar(level, element->bar);
				break;
			case PATTERN_USAGE_ELEMENT:
				_output(level, "Use(pattern=%s, repetitions=%d)\n",
					element->patternUsage->name, element->patternUsage->repetitions);
				break;
			default:
				logError(_logger, "Unknown content element type: %d", element->type);
				break;
		}
	}
}

static void _printSection(const unsigned int level, Section * section) {
	_output(level, "Section(name=%s, repetitions=%d)\n", section->name, section->repetitions);
	switch (section->bodyType) {
		case CONTENT_BODY:
			_printContent(1 + level, section->content);
			break;
		case TRACKS_BODY:
			for (Track * track = section->trackList->first; track != NULL; track = track->next) {
				_output(1 + level, "Track(instrument=%s)\n", track->instrument);
				_printContent(2 + level, track->content);
			}
			break;
	}
}

static void _printSongElement(const unsigned int level, SongElement * songElement) {
	switch (songElement->type) {
		case AUTHOR_ELEMENT:
			_output(level, "Author(\"%s\")\n", songElement->text);
			break;
		case COPYRIGHT_ELEMENT:
			_output(level, "Copyright(\"%s\")\n", songElement->text);
			break;
		case KEY_ELEMENT:
			_output(level, "Key(note=%s, mode=%s)\n", songElement->key->note,
				songElement->key->mode == MAJOR_MODE ? "major" : "minor");
			break;
		case PATTERN_ELEMENT:
			_output(level, "Pattern(name=%s)\n", songElement->pattern->name);
			_printContent(1 + level, songElement->pattern->content);
			break;
		case SECTION_ELEMENT:
			_printSection(level, songElement->section);
			break;
		case TEMPO_ELEMENT:
			_output(level, "Tempo(%d)\n", songElement->tempo);
			break;
		case TIME_SIGNATURE_ELEMENT:
			_output(level, "TimeSignature(%d/%d)\n",
				songElement->timeSignature->numerator,
				songElement->timeSignature->denominator);
			break;
		default:
			logError(_logger, "Unknown song element type: %d", songElement->type);
			break;
	}
}

static void _printSong(const unsigned int level, Song * song) {
	_output(level, "Song(name=\"%s\")\n", song->name);
	for (SongElement * element = song->firstElement; element != NULL; element = element->next) {
		_printSongElement(1 + level, element);
	}
}

/**
 * Generates an indentation string for the specified level.
 */
static char * _indentation(const unsigned int level) {
	return indentation(_indentationCharacter, level, _indentationSize);
}

/**
 * Outputs a formatted string to standard output. The "fflush" instruction
 * allows to see the output even close to a failure, because it drops the
 * buffering.
 */
static void _output(const unsigned int indentationLevel, const char * const format, ...) {
	va_list arguments;
	va_start(arguments, format);
	char * indentation = _indentation(indentationLevel);
	char * effectiveFormat = concatenate(2, indentation, format);
	vfprintf(stdout, effectiveFormat, arguments);
	fflush(stdout);
	free(effectiveFormat);
	free(indentation);
	va_end(arguments);
}

/** PUBLIC FUNCTIONS */

void executeGenerator(CompilerState * compilerState) {
	logDebugging(_logger, "Printing the AST...");
	Program * program = compilerState->abstractSyntaxtTree;
	_output(0, "%s", "Program\n");
	_printSong(1, program->song);
	logDebugging(_logger, "Generation is done.");
}
