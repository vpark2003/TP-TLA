#ifndef GENERATOR_HEADER
#define GENERATOR_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../domain/Music.h"
#include "../semantic-analysis/SymbolTable.h"
#include <stdio.h>

/** Default output file, if none is provided as argument. */
#define DEFAULT_OUTPUT_PATH "output.mid"

/** Initialize module's internal state. */
ModuleDestructor initializeGeneratorModule();

/**
 * Generates a Standard MIDI File (format 1) from the AST, using the symbol
 * table built by the semantic analysis to expand the patterns, and writes it
 * in "compilerState->outputPath".
 */
CompilationStatus executeGenerator(CompilerState * compilerState);

#endif
