%{

#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonActions.h"

/**
 * The error reporting function for Bison parser.
 *
 * @todo Add location to the grammar and "pushToken" API function.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Error-Reporting-Function.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Tracking-Locations.html
 */
void yyerror(const YYLTYPE * location, const char * message) {
	SyntaxErrorAction(message);
}

%}

// You touch this, and you die.
%define api.pure full
%define api.push-pull push
%define api.value.union.name SemanticValue
%define parse.error detailed
%locations

%union {
	/** Terminals. */

	signed int integer;
	char * string;
	Chord * chord;
	TokenLabel token;

	/** Non-terminals. */

	Bar * bar;
	ChordEntry * chordEntry;
	Content * content;
	Mode mode;
	Pattern * pattern;
	PatternUsage * patternUsage;
	Program * program;
	Section * section;
	Song * song;
	SongElement * songElement;
	Track * track;
	TrackList * trackList;
}

/**
 * Destructors. This functions are executed after the parsing ends, so if the
 * AST must be used in the following phases of the compiler you shouldn't used
 * this approach for the AST root node ("program" non-terminal, in this
 * grammar), or it will drop the entire tree even if the parsing succeeds.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Destructor-Decl.html
 */
%destructor { free($$); } <string>
%destructor { destroyChord($$); } <chord>
%destructor { destroyBar($$); } <bar>
%destructor { destroyChordEntry($$); } <chordEntry>
%destructor { destroyContent($$); } <content>
%destructor { destroyPattern($$); } <pattern>
%destructor { destroyPatternUsage($$); } <patternUsage>
%destructor { destroySection($$); } <section>
%destructor { destroySong($$); } <song>
%destructor { destroySongElement($$); } <songElement>
%destructor { destroyTrack($$); } <track>
%destructor { destroyTrackList($$); } <trackList>

/** Terminals. */
%token <integer> INTEGER
%token <integer> REPETITION
%token <string> IDENTIFIER
%token <string> NOTE
%token <string> STRING
%token <chord> CHORD

%token <token> AUTHOR
%token <token> COPYRIGHT
%token <token> KEY_KEYWORD
%token <token> MAJOR
%token <token> MINOR
%token <token> PATTERN
%token <token> SECTION
%token <token> SONG
%token <token> TEMPO
%token <token> TIME
%token <token> TRACK
%token <token> USE

%token <token> CLOSE_BRACE
%token <token> CLOSE_PARENTHESIS
%token <token> NO_CHORD
%token <token> OPEN_BRACE
%token <token> OPEN_PARENTHESIS
%token <token> PIPE
%token <token> SLASH

%token <token> CLOSE_COMMENT
%token <token> OPEN_COMMENT
%token <token> IGNORED
%token <token> UNKNOWN

/** Non-terminals. */
%type <bar> bar
%type <chord> chord
%type <chordEntry> chord_entry
%type <content> bar_line
%type <content> bars
%type <content> content
%type <integer> repetition
%type <mode> mode
%type <pattern> pattern
%type <patternUsage> pattern_usage
%type <program> program
%type <section> section
%type <song> song
%type <song> song_elements
%type <songElement> song_element
%type <track> track
%type <trackList> tracks

%%

// IMPORTANT: To use λ in the following grammar, use the %empty symbol.

program: song																{ $$ = SongProgramSemanticAction($1); }
	;

song: SONG STRING OPEN_BRACE song_elements CLOSE_BRACE						{ $$ = SongSemanticAction($2, $4); }
	;

song_elements: %empty														{ $$ = EmptySongSemanticAction(); }
	| song_elements song_element											{ $$ = AppendSongElementSemanticAction($1, $2); }
	;

song_element: TEMPO INTEGER													{ $$ = TempoSongElementSemanticAction($2); }
	| TIME INTEGER[numerator] SLASH INTEGER[denominator]					{ $$ = TimeSignatureSongElementSemanticAction($numerator, $denominator); }
	| KEY_KEYWORD NOTE mode													{ $$ = KeySongElementSemanticAction($2, $3); }
	| AUTHOR STRING															{ $$ = TextSongElementSemanticAction($2, AUTHOR_ELEMENT); }
	| COPYRIGHT STRING														{ $$ = TextSongElementSemanticAction($2, COPYRIGHT_ELEMENT); }
	| pattern																{ $$ = PatternSongElementSemanticAction($1); }
	| section																{ $$ = SectionSongElementSemanticAction($1); }
	;

mode: MAJOR																	{ $$ = MAJOR_MODE; }
	| MINOR																	{ $$ = MINOR_MODE; }
	;

pattern: PATTERN IDENTIFIER OPEN_BRACE content CLOSE_BRACE					{ $$ = PatternSemanticAction($2, $4); }
	;

section: SECTION IDENTIFIER repetition OPEN_BRACE content CLOSE_BRACE		{ $$ = ContentSectionSemanticAction($2, $3, $5); }
	| SECTION IDENTIFIER repetition OPEN_BRACE tracks CLOSE_BRACE			{ $$ = TracksSectionSemanticAction($2, $3, $5); }
	;

repetition: %empty															{ $$ = NO_REPETITIONS; }
	| REPETITION															{ $$ = $1; }
	;

tracks: track																{ $$ = TrackListSemanticAction($1); }
	| tracks track															{ $$ = AppendTrackSemanticAction($1, $2); }
	;

track: TRACK IDENTIFIER OPEN_BRACE content CLOSE_BRACE						{ $$ = TrackSemanticAction($2, $4); }
	;

content: bar_line															{ $$ = $1; }
	| pattern_usage															{ $$ = PatternUsageContentSemanticAction(NULL, $1); }
	| content bar_line														{ $$ = ConcatenateContentSemanticAction($1, $2); }
	| content pattern_usage													{ $$ = PatternUsageContentSemanticAction($1, $2); }
	;

pattern_usage: USE IDENTIFIER repetition									{ $$ = PatternUsageSemanticAction($2, $3); }
	;

bar_line: PIPE bars															{ $$ = $2; }
	;

bars: bar PIPE																{ $$ = BarContentSemanticAction(NULL, $1); }
	| bars bar PIPE															{ $$ = BarContentSemanticAction($1, $2); }
	;

bar: chord_entry															{ $$ = BarSemanticAction(NULL, $1); }
	| bar chord_entry														{ $$ = BarSemanticAction($1, $2); }
	;

chord_entry: chord															{ $$ = ChordEntrySemanticAction($1, false, 0); }
	| chord OPEN_PARENTHESIS INTEGER CLOSE_PARENTHESIS						{ $$ = ChordEntrySemanticAction($1, true, $3); }
	;

chord: CHORD																{ $$ = $1; }
	| NO_CHORD																{ $$ = NoChordSemanticAction(); }
	;

%%
