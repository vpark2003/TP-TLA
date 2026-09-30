#include "AbstractSyntaxTree.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownAbstractSyntaxTreeModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: AbstractSyntaxTree...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeAbstractSyntaxTreeModule() {
	_logger = createLogger("AbstractSyntaxTree");
	return _shutdownAbstractSyntaxTreeModule;
}

/* PUBLIC FUNCTIONS */

void destroyBar(Bar * bar) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (bar != NULL) {
		ChordEntry * entry = bar->firstEntry;
		while (entry != NULL) {
			ChordEntry * next = entry->next;
			destroyChordEntry(entry);
			entry = next;
		}
		free(bar);
	}
}

void destroyChord(Chord * chord) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (chord != NULL) {
		free(chord->symbol);
		free(chord->root);
		free(chord->quality);
		free(chord->bass);
		free(chord);
	}
}

void destroyChordEntry(ChordEntry * chordEntry) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (chordEntry != NULL) {
		destroyChord(chordEntry->chord);
		free(chordEntry);
	}
}

void destroyContent(Content * content) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (content != NULL) {
		ContentElement * element = content->first;
		while (element != NULL) {
			ContentElement * next = element->next;
			destroyContentElement(element);
			element = next;
		}
		free(content);
	}
}

void destroyContentElement(ContentElement * contentElement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (contentElement != NULL) {
		switch (contentElement->type) {
			case BAR_ELEMENT:
				destroyBar(contentElement->bar);
				break;
			case PATTERN_USAGE_ELEMENT:
				destroyPatternUsage(contentElement->patternUsage);
				break;
		}
		free(contentElement);
	}
}

void destroyKey(Key * key) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (key != NULL) {
		free(key->note);
		free(key);
	}
}

void destroyPattern(Pattern * pattern) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (pattern != NULL) {
		free(pattern->name);
		destroyContent(pattern->content);
		free(pattern);
	}
}

void destroyPatternUsage(PatternUsage * patternUsage) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (patternUsage != NULL) {
		free(patternUsage->name);
		free(patternUsage);
	}
}

void destroyProgram(Program * program) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (program != NULL) {
		destroySong(program->song);
		free(program);
	}
}

void destroySection(Section * section) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (section != NULL) {
		free(section->name);
		switch (section->bodyType) {
			case CONTENT_BODY:
				destroyContent(section->content);
				break;
			case TRACKS_BODY:
				destroyTrackList(section->trackList);
				break;
		}
		free(section);
	}
}

void destroySong(Song * song) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (song != NULL) {
		free(song->name);
		SongElement * element = song->firstElement;
		while (element != NULL) {
			SongElement * next = element->next;
			destroySongElement(element);
			element = next;
		}
		free(song);
	}
}

void destroySongElement(SongElement * songElement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (songElement != NULL) {
		switch (songElement->type) {
			case AUTHOR_ELEMENT:
			case COPYRIGHT_ELEMENT:
				free(songElement->text);
				break;
			case KEY_ELEMENT:
				destroyKey(songElement->key);
				break;
			case PATTERN_ELEMENT:
				destroyPattern(songElement->pattern);
				break;
			case SECTION_ELEMENT:
				destroySection(songElement->section);
				break;
			case TEMPO_ELEMENT:
				break;
			case TIME_SIGNATURE_ELEMENT:
				destroyTimeSignature(songElement->timeSignature);
				break;
		}
		free(songElement);
	}
}

void destroyTimeSignature(TimeSignature * timeSignature) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (timeSignature != NULL) {
		free(timeSignature);
	}
}

void destroyTrack(Track * track) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (track != NULL) {
		free(track->instrument);
		destroyContent(track->content);
		free(track);
	}
}

void destroyTrackList(TrackList * trackList) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (trackList != NULL) {
		Track * track = trackList->first;
		while (track != NULL) {
			Track * next = track->next;
			destroyTrack(track);
			track = next;
		}
		free(trackList);
	}
}
