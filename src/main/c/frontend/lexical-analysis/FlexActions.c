#include "FlexActions.h"

/* MODULE INTERNAL STATE */

static bool _logIgnoredLexemes = true;
static LexicalAnalyzer * _lexicalAnalyzer = NULL;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownFlexActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: FlexActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_lexicalAnalyzer = NULL;
}

ModuleDestructor initializeFlexActionsModule(LexicalAnalyzer * lexicalAnalyzer) {
	_lexicalAnalyzer = lexicalAnalyzer;
	_logger = createLogger("FlexActions");
	_logIgnoredLexemes = getBooleanOrDefault("LOG_IGNORED_LEXEMES", _logIgnoredLexemes);
	return _shutdownFlexActionsModule;
}

/* PRIVATE FUNCTIONS */

static void _logTokenAction(const char * actionName, Token * token);
static char * _substring(const char * string, const unsigned int start, const unsigned int length);
static CompilationStatus _pushAndDestroy(const char * actionName, Token * token);

/**
 * Logs a lexical-analyzer action over a token in DEBUGGING level.
 */
static void _logTokenAction(const char * actionName, Token * token) {
	char * _lexeme = escape(token->lexeme);
	logDebugging(_logger, WARNING_COLOR "%s" DEFAULT_COLOR ": Token(context=%d, label=%d, length=%d, lexeme=%s\"%s\"%s, line=%d, semanticValue=%p)",
		actionName,
		token->context,
		token->label,
		token->length,
		INFORMATION_COLOR, _lexeme, DEFAULT_COLOR,
		token->line,
		token->semanticValue);
	free(_lexeme);
	_lexeme = NULL;
}

/**
 * Logs, pushes the token into the parser, and releases the token (but not the
 * heap-memory referenced by its semantic value, that is now owned by Bison).
 */
static CompilationStatus _pushAndDestroy(const char * actionName, Token * token) {
	_logTokenAction(actionName, token);
	CompilationStatus status = pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return status;
}

/**
 * Returns a new heap-allocated copy of a portion of a string.
 */
static char * _substring(const char * string, const unsigned int start, const unsigned int length) {
	char * result = calloc(length + 1, sizeof(char));
	strncpy(result, string + start, length);
	return result;
}

/* PUBLIC FUNCTIONS */

/**
 * The lexeme was already validated by the "chord" pattern in Flex, so its
 * structure is known: a root note (1 or 2 characters), an optional quality
 * and, optionally, "/" followed by the bass note. Here we only split it into
 * its components so the AST can store them separately.
 */
CompilationStatus ChordLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, CHORD);
	const char * lexeme = token->lexeme;
	const unsigned int length = token->length;
	const unsigned int rootLength = (1 < length && (lexeme[1] == '#' || lexeme[1] == 'b')) ? 2 : 1;
	const char * slash = strchr(lexeme, '/');
	const unsigned int qualityEnd = (slash == NULL) ? length : (unsigned int) (slash - lexeme);
	Chord * chord = calloc(1, sizeof(Chord));
	chord->type = CHORD_SYMBOL;
	chord->symbol = _substring(lexeme, 0, length);
	chord->root = _substring(lexeme, 0, rootLength);
	chord->quality = _substring(lexeme, rootLength, qualityEnd - rootLength);
	chord->bass = (slash == NULL) ? NULL : _substring(slash, 1, strlen(slash) - 1);
	token->semanticValue->chord = chord;
	return _pushAndDestroy(__FUNCTION__, token);
}

CompilationStatus EnterKeyLexemeAction(FlexContext context) {
	Token * token = createToken(_lexicalAnalyzer, KEY_KEYWORD);
	token->semanticValue->token = KEY_KEYWORD;
	CompilationStatus status = _pushAndDestroy(__FUNCTION__, token);
	enterLexicalAnalyzerContext(_lexicalAnalyzer, context);
	return status;
}

CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context) {
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, OPEN_COMMENT);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	enterLexicalAnalyzerContext(_lexicalAnalyzer, context);
	return IN_PROGRESS;
}

CompilationStatus EOFLexemeAction() {
	CompilationStatus status = IN_PROGRESS;
	Token * token = createToken(_lexicalAnalyzer, 0);
	_logTokenAction(__FUNCTION__, token);
	if (!popInputBuffer(_lexicalAnalyzer)) {
		status = pushToken(_lexicalAnalyzer, token);
		FlexContext context = currentLexicalAnalyzerContext(_lexicalAnalyzer);
		if (0 < context) {
			logError(_logger, "The final context is not closed (context=%d).", context);
			status = FAILED;
		}
	}
	destroyToken(token);
	return status;
}

CompilationStatus IdentifierLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, IDENTIFIER);
	token->semanticValue->string = _substring(token->lexeme, 0, token->length);
	return _pushAndDestroy(__FUNCTION__, token);
}

CompilationStatus IgnoredLexemeAction() {
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, IGNORED);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	return IN_PROGRESS;
}

CompilationStatus IntegerLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, INTEGER);
	token->semanticValue->integer = atoi(token->lexeme);
	return _pushAndDestroy(__FUNCTION__, token);
}

CompilationStatus LeaveKeyLexemeAction(TokenLabel label) {
	Token * token = createToken(_lexicalAnalyzer, label);
	token->semanticValue->token = label;
	CompilationStatus status = _pushAndDestroy(__FUNCTION__, token);
	leaveLexicalAnalyzerContext(_lexicalAnalyzer);
	return status;
}

CompilationStatus LeaveMultilineCommentLexemeAction() {
	leaveLexicalAnalyzerContext(_lexicalAnalyzer);
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, CLOSE_COMMENT);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	return IN_PROGRESS;
}

CompilationStatus NoteLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, NOTE);
	token->semanticValue->string = _substring(token->lexeme, 0, token->length);
	return _pushAndDestroy(__FUNCTION__, token);
}

/**
 * Lexeme "xN": the value of N is kept as the semantic value. Validating that
 * N is a positive natural number is a job for the semantic analysis.
 */
CompilationStatus RepetitionLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, REPETITION);
	token->semanticValue->integer = atoi(token->lexeme + 1);
	return _pushAndDestroy(__FUNCTION__, token);
}

/**
 * Lexeme "...": the semantic value is the content without the quotes.
 */
CompilationStatus StringLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, STRING);
	token->semanticValue->string = _substring(token->lexeme, 1, token->length - 2);
	return _pushAndDestroy(__FUNCTION__, token);
}

CompilationStatus TokenLexemeAction(TokenLabel label) {
	Token * token = createToken(_lexicalAnalyzer, label);
	token->semanticValue->token = label;
	return _pushAndDestroy(__FUNCTION__, token);
}

CompilationStatus UnknownLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, UNKNOWN);
	_logTokenAction(__FUNCTION__, token);
	logError(_logger, "Unknown lexeme \"%s\" at line %d.", token->lexeme, token->line);
	// Pushing the UNKNOWN token forces Bison to abort the parsing and release
	// (through the "%destructor" directives) every semantic value in its stack.
	pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return FAILED;
}
