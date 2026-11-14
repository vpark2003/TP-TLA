#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdbool.h>

typedef enum {
    SYMBOL_PATTERN,
    SYMBOL_SECTION,
    SYMBOL_INSTRUMENT
} SymbolType;

typedef struct Symbol {
    char * name;
    SymbolType type;
    void * definition;
    struct Symbol * next;
} Symbol;

typedef struct SymbolTable {
    Symbol * first;
} SymbolTable;

SymbolTable *createSymbolTable(void);

bool insertSymbol(SymbolTable *table, const char *name, SymbolType type, void *definition);

Symbol *findSymbol(SymbolTable *table, const char *name, SymbolType type);

void freeSymbolTable(SymbolTable *table);

#endif