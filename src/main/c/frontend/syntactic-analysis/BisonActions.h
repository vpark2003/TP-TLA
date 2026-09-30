#ifndef BISON_ACTIONS_HEADER
#define BISON_ACTIONS_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonParser.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * Value used by the "repetition" non-terminal when a section or a pattern
 * usage does not specify "xN".
 */
#define NO_REPETITIONS -1

/** Initialize module's internal state. */
ModuleDestructor initializeBisonActionsModule();

/**
 * Error reporting.
 */

void SyntaxErrorAction(const char * message);

/**
 * Bison semantic actions.
 */

Song * AppendSongElementSemanticAction(Song * song, SongElement * songElement);
TrackList * AppendTrackSemanticAction(TrackList * trackList, Track * track);
Content * BarContentSemanticAction(Content * content, Bar * bar);
Bar * BarSemanticAction(Bar * bar, ChordEntry * chordEntry);
ChordEntry * ChordEntrySemanticAction(Chord * chord, const bool hasDuration, const int duration);
Content * ConcatenateContentSemanticAction(Content * content, Content * tail);
Section * ContentSectionSemanticAction(char * name, const int repetitions, Content * content);
Song * EmptySongSemanticAction();
SongElement * KeySongElementSemanticAction(char * note, const Mode mode);
Chord * NoChordSemanticAction();
Pattern * PatternSemanticAction(char * name, Content * content);
SongElement * PatternSongElementSemanticAction(Pattern * pattern);
Content * PatternUsageContentSemanticAction(Content * content, PatternUsage * patternUsage);
PatternUsage * PatternUsageSemanticAction(char * name, const int repetitions);
SongElement * SectionSongElementSemanticAction(Section * section);
Program * SongProgramSemanticAction(Song * song);
Song * SongSemanticAction(char * name, Song * song);
SongElement * TempoSongElementSemanticAction(const int tempo);
SongElement * TextSongElementSemanticAction(char * text, const SongElementType type);
SongElement * TimeSignatureSongElementSemanticAction(const int numerator, const int denominator);
TrackList * TrackListSemanticAction(Track * track);
Section * TracksSectionSemanticAction(char * name, const int repetitions, TrackList * trackList);
Track * TrackSemanticAction(char * instrument, Content * content);

#endif
