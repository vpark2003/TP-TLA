#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include <stdbool.h>
#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/type/CompilerState.h"

bool executeSemanticAnalysis(CompilerState * compilerState);

#endif