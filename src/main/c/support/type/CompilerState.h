#ifndef COMPILER_STATE_HEADER
#define COMPILER_STATE_HEADER
#include "../../backend/semantic-analysis/SymbolTable.h"
/**
 * The global state of the compiler. Should transport every data structure
 * needed across the different phases of a compilation.
 */
typedef struct {
	/**
	 * The root node of the AST.
	 */
	void * abstractSyntaxtTree;
	SymbolTable *symbolTable; // TODO (Stage III): Add a symbol table (patterns, sections, instruments).
	// TODO (Stage III): Add the semantic information needed to generate the MIDI.

	/**
	 * Path of the MIDI file to generate.
	 */
	const char * outputPath;
} CompilerState;

#endif
