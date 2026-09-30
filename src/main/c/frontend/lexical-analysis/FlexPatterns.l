%top{

#include "../../support/type/CompilationStatus.h"
#include "../../support/type/FlexContext.h"
#include "../../support/type/TokenLabel.h"
#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include "../syntactic-analysis/BisonParser.h"

}

%{

/**
 * @see https://westes.github.io/flex/manual/Definitions-Section.html#Definitions-Section
 */

extern CompilationStatus ChordLexemeAction();
extern CompilationStatus EnterKeyLexemeAction(FlexContext context);
extern CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context);
extern CompilationStatus EOFLexemeAction();
extern CompilationStatus IdentifierLexemeAction();
extern CompilationStatus IgnoredLexemeAction();
extern CompilationStatus IntegerLexemeAction();
extern CompilationStatus LeaveKeyLexemeAction(TokenLabel label);
extern CompilationStatus LeaveMultilineCommentLexemeAction();
extern CompilationStatus NoteLexemeAction();
extern CompilationStatus RepetitionLexemeAction();
extern CompilationStatus StringLexemeAction();
extern CompilationStatus TokenLexemeAction(TokenLabel label);
extern CompilationStatus UnknownLexemeAction();

%}

/**
 * Flex contexts (a.k.a. start conditions).
 *
 * KEY: after the "key" keyword only a note and a mode (major/minor) are valid.
 * This way, in "key C major" the "C" is a NOTE and not a CHORD.
 *
 * @see https://westes.github.io/flex/manual/Start-Conditions.html
 */
%x KEY
%x MULTILINE_COMMENT

/**
 * Reusable patterns.
 *
 * A chord is: root note + optional quality/extensions + optional bass note
 * (slash chord). For example: C, Am, F#7, Bb, Cmaj7, Am7b5, Gsus4, C/E, Dm/F.
 * Identifiers always start with a lowercase letter, so they never collide
 * with chords (which always start with an uppercase note, A to G).
 *
 * @see https://westes.github.io/flex/manual/Matching.html
 * @see https://westes.github.io/flex/manual/Patterns.html
 */
whitespace			[[:space:]]+
note				[A-G][#b]?
quality				(maj|min|m|M|dim|aug|sus|add|\+|[0-9]|#|b)*
chord				{note}{quality}("/"{note})?
identifier			[a-z][a-zA-Z0-9_]*

%%

%{

/**
 * @see https://westes.github.io/flex/manual/Rules-Section.html#Rules-Section
 */

%}

"/*"								{ return EnterMultilineCommentLexemeAction(MULTILINE_COMMENT); }
<MULTILINE_COMMENT>"*/"				{ return LeaveMultilineCommentLexemeAction(); }
<MULTILINE_COMMENT>[^*]+			{ return IgnoredLexemeAction(); }
<MULTILINE_COMMENT>.				{ return IgnoredLexemeAction(); }

"//"[^\n]*							{ return IgnoredLexemeAction(); }

"key"								{ return EnterKeyLexemeAction(KEY); }
<KEY>{note}							{ return NoteLexemeAction(); }
<KEY>"major"						{ return LeaveKeyLexemeAction(MAJOR); }
<KEY>"minor"						{ return LeaveKeyLexemeAction(MINOR); }
<KEY>{whitespace}					{ return IgnoredLexemeAction(); }
<KEY>.								{ return UnknownLexemeAction(); }

"song"								{ return TokenLexemeAction(SONG); }
"tempo"								{ return TokenLexemeAction(TEMPO); }
"time"								{ return TokenLexemeAction(TIME); }
"author"							{ return TokenLexemeAction(AUTHOR); }
"copyright"							{ return TokenLexemeAction(COPYRIGHT); }
"section"							{ return TokenLexemeAction(SECTION); }
"pattern"							{ return TokenLexemeAction(PATTERN); }
"track"								{ return TokenLexemeAction(TRACK); }
"use"								{ return TokenLexemeAction(USE); }

"{"									{ return TokenLexemeAction(OPEN_BRACE); }
"}"									{ return TokenLexemeAction(CLOSE_BRACE); }
"("									{ return TokenLexemeAction(OPEN_PARENTHESIS); }
")"									{ return TokenLexemeAction(CLOSE_PARENTHESIS); }
"|"									{ return TokenLexemeAction(PIPE); }
"/"									{ return TokenLexemeAction(SLASH); }

"N.C."								{ return TokenLexemeAction(NO_CHORD); }
{chord}								{ return ChordLexemeAction(); }

"x"[[:digit:]]+						{ return RepetitionLexemeAction(); }
{identifier}						{ return IdentifierLexemeAction(); }
[[:digit:]]+						{ return IntegerLexemeAction(); }
\"[^"\n]*\"							{ return StringLexemeAction(); }

{whitespace}						{ return IgnoredLexemeAction(); }
.									{ return UnknownLexemeAction(); }

<<EOF>>								{ return EOFLexemeAction(); }

%%

/**
 * @see https://westes.github.io/flex/manual/User-Code-Section.html#User-Code-Section
 */

#include "FlexActions.h"
#include "FlexExport.h"
