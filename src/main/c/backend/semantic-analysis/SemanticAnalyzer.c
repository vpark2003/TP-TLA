
#include "SemanticAnalyzer.h"
#include "SymbolTable.h"

/* Verifica que el patrón utilizado exista */

static bool _validatePatternUsage(PatternUsage *usage, SymbolTable *table) {

    if (usage == NULL) {
        return false;
    }

    return findSymbol(table, usage->name, SYMBOL_PATTERN) != NULL;
}

static bool _validateChordEntries(ChordEntry *entry) {

    if (entry == NULL) {
        return true;
    }

    if (entry->hasDuration && entry->duration <= 0) {
        return false;
    }

    return _validateChordEntries(entry->next);
}

static bool _validateBar(Bar *bar) {

    if (bar == NULL) {
        return false;
    }

    return _validateChordEntries(bar->firstEntry);
}

/* Recorre recursivamente los elementos de un Content */

static bool _validateContentElement(ContentElement *element, SymbolTable *table) {

    if (element == NULL) {
        return true;
    }

    if (element->type == PATTERN_USAGE_ELEMENT) {

        if (!_validatePatternUsage(element->patternUsage, table)) {
            return false;
        }
    }
    else if(element->type == BAR_ELEMENT) {

        if (!_validateBar(element->bar)) {
            return false;
        }
    }

    return _validateContentElement(element->next, table);
}

/* Valida el contenido de un patrón o de una pista */

static bool _validateContent(Content *content, SymbolTable *table) {

    if (content == NULL) {
        return false;
    }

    return _validateContentElement(content->first, table);
}

/* Recorre recursivamente los tracks de una sección */

static bool _validateTracks(Track *track, SymbolTable *table) {

    if (track == NULL) {
        return true;
    }

    if (!_validateContent(track->content, table)) {
        return false;
    }

    return _validateTracks(track->next, table);
}

/* Valida el contenido de una sección */

static bool _validateSection(Section *section, SymbolTable *table) {

    if (section == NULL) {
        return false;
    }

    if (section->bodyType == CONTENT_BODY) {
        return _validateContent(section->content, table);
    }

    if (section->bodyType == TRACKS_BODY) {

        if (section->trackList == NULL) {
            return false;
        }

        return _validateTracks(section->trackList->first, table);
    }

    return false;
}

/* Segunda pasada: valida los use de patrones y secciones */
static bool _validateTimeSignature(TimeSignature *timeSignature) {

    if (timeSignature == NULL) {
        return false;
    }

    if (timeSignature->numerator <= 0 ||
        timeSignature->denominator <= 0) {
        return false;
    }

    int denominator = timeSignature->denominator;

    while (denominator % 2 == 0) {
        denominator = denominator / 2;
    }

    return denominator == 1;
}

static bool _validateSongElements(SongElement *element, SymbolTable *table) {

    if (element == NULL) {
        return true;
    }
    
    switch (element->type) {

        case PATTERN_ELEMENT:
            if (!_validateContent(element->pattern->content, table)) {
                return false;
            }
            break;

        case SECTION_ELEMENT:
            if (!_validateSection(element->section, table)) {
                return false;
            }
            break;

        case TIME_SIGNATURE_ELEMENT:
            if (!_validateTimeSignature(element->timeSignature)) {
                return false;
            }
            break;

        default:
            break;
    }

    return _validateSongElements(element->next, table);
}

/* Función principal del análisis semántico */

bool executeSemanticAnalysis(CompilerState *compilerState) {

    if (compilerState == NULL ||
        compilerState->abstractSyntaxtTree == NULL) {
        return false;
    }

    Program *program = compilerState->abstractSyntaxtTree;

    if (program->song == NULL) {
        return false;
    }

    compilerState->symbolTable = createSymbolTable();

    if (compilerState->symbolTable == NULL) {
        return false;
    }

    Song *song = program->song;

    /* PRIMERA PASADA: registrar patrones y secciones */

    for (SongElement *element = song->firstElement;
         element != NULL;
         element = element->next) {

        switch (element->type) {

            case PATTERN_ELEMENT:
                if (!insertSymbol(
                        compilerState->symbolTable,
                        element->pattern->name,
                        SYMBOL_PATTERN,
                        element->pattern)) {
                    return false;
                }
                break;

            case SECTION_ELEMENT:
                if (!insertSymbol(
                        compilerState->symbolTable,
                        element->section->name,
                        SYMBOL_SECTION,
                        element->section)) {
                    return false;
                }
                break;

            default:
                break;
        }
    }

    /* SEGUNDA PASADA: validar referencias a patrones */

    if (!_validateSongElements(song->firstElement,
                               compilerState->symbolTable)) {
        return false;
    }

    return true;
}
