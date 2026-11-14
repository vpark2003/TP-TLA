#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include <stdbool.h>
#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../domain/Music.h"

/* Límites del lenguaje (validados en el análisis semántico). */
#define MIN_TEMPO 20
#define MAX_TEMPO 400
#define MAX_NUMERATOR 32
#define MAX_DENOMINATOR 32
#define MAX_REPETITIONS 100
#define MAX_SONG_BARS 10000
/* MIDI tiene 16 canales, pero el 10 está reservado para percusión. */
#define MAX_INSTRUMENTS 15

/* Inicializa el estado interno del módulo. */
ModuleDestructor initializeSemanticAnalyzerModule();

/*
 * Valida el AST y construye la tabla de símbolos en
 * "compilerState->symbolTable". Informa cada error encontrado y devuelve
 * false si el programa debe rechazarse.
 */
bool executeSemanticAnalysis(CompilerState * compilerState);

#endif
