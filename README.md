[![✗](https://img.shields.io/badge/Release-v2.0.0-ffb600.svg?style=for-the-badge)](https://github.com/vpark2003/TP-TLA/releases)

[![✗](https://github.com/vpark2003/TP-TLA/actions/workflows/pipeline.yaml/badge.svg?branch=development)](https://github.com/vpark2003/TP-TLA/actions/workflows/pipeline.yaml)

# B#

Lenguaje de dominio específico (DSL, *Domain Specific Language*) para describir la estructura armónica de canciones en formato *lead sheet*. Grupo **G-141**.

* [Estado: Stage II](#estado-stage-ii)
* [Ejemplo](#ejemplo)
* [Gramática](#gramática)
* [Decisiones de diseño](#decisiones-de-diseño)
* [Casos de prueba](#casos-de-prueba)

## Estado: Stage II

El *frontend* está completo: el analizador léxico (Flex) transforma el programa en un *stream* de *tokens*, y el analizador sintáctico (Bison) construye el AST (Árbol de Sintaxis Abstracta). Mientras no exista el *backend*, el compilador imprime el AST por salida estándar:

```bash
src/main/bash/build.sh
LOGGING_LEVEL=ERROR src/main/bash/run.sh src/test/c/accept/31-full-song-extended.bsharp
src/main/bash/test.sh
```

El análisis semántico (duración de los compases, repeticiones positivas, patrones definidos, instrumentos válidos, etc.) y la generación del archivo MIDI (*Musical Instrument Digital Interface*) corresponden al Stage III.

## Ejemplo

```bsharp
// B# - G-141
song "Summer Lights" {
    author "B# Team"
    copyright "CC BY 4.0"
    tempo 128
    time 4/4
    key C major

    pattern turnaround {
        | Fmaj7 | G7 | Em7 | Am7 |
    }

    section verse {
        | C(2) G(2) | Am(2) F(2) | C/E | G7 |
    }

    section chorus x2 {
        track piano {
            use turnaround
            | N.C. | G7 | C | C |
        }
        track bass {
            | F | G | E | A |
            | N.C. | G | C | C |
        }
    }
}
```

## Gramática

G = ⟨Σ, N, Π, S⟩, con S = `program`.

**Σ (definido por Flex, en `FlexPatterns.l`):**

| Token | Lexema | Valor semántico |
| :-- | :-- | :-- |
| `SONG`, `TEMPO`, `TIME`, `KEY_KEYWORD`, `AUTHOR`, `COPYRIGHT`, `SECTION`, `PATTERN`, `TRACK`, `USE` | `song`, `tempo`, `time`, `key`, `author`, `copyright`, `section`, `pattern`, `track`, `use` | — |
| `MAJOR`, `MINOR` | `major`, `minor` (solo después de `key`) | — |
| `NOTE` | `[A-G][#b]?` (solo después de `key`) | `char *` |
| `CHORD` | nota + cualidad/extensiones opcionales + `/nota` opcional (`C`, `Am7b5`, `F#7`, `C/E`) | `Chord *` (raíz, cualidad, bajo) |
| `NO_CHORD` | `N.C.` | — |
| `REPETITION` | `x[0-9]+` | `int` |
| `INTEGER` | `[0-9]+` | `int` |
| `STRING` | `"..."` (sin saltos de línea) | `char *` sin comillas |
| `IDENTIFIER` | `[a-z][a-zA-Z0-9_]*` | `char *` |
| `OPEN_BRACE`, `CLOSE_BRACE`, `OPEN_PARENTHESIS`, `CLOSE_PARENTHESIS`, `PIPE`, `SLASH` | `{` `}` `(` `)` `\|` `/` | — |

Los espacios en blanco y los comentarios (`// ...` y `/* ... */`) se descartan en el análisis léxico.

**N y Π (definidos por Bison, en `BisonGrammar.y`):**

```
program       → song
song          → SONG STRING OPEN_BRACE song_elements CLOSE_BRACE
song_elements → λ | song_elements song_element
song_element  → TEMPO INTEGER
              | TIME INTEGER SLASH INTEGER
              | KEY_KEYWORD NOTE mode
              | AUTHOR STRING
              | COPYRIGHT STRING
              | pattern
              | section
mode          → MAJOR | MINOR
pattern       → PATTERN IDENTIFIER OPEN_BRACE content CLOSE_BRACE
section       → SECTION IDENTIFIER repetition OPEN_BRACE content CLOSE_BRACE
              | SECTION IDENTIFIER repetition OPEN_BRACE tracks CLOSE_BRACE
repetition    → λ | REPETITION
tracks        → track | tracks track
track         → TRACK IDENTIFIER OPEN_BRACE content CLOSE_BRACE
content       → bar_line | pattern_usage | content bar_line | content pattern_usage
pattern_usage → USE IDENTIFIER repetition
bar_line      → PIPE bars
bars          → bar PIPE | bars bar PIPE
bar           → chord_entry | bar chord_entry
chord_entry   → chord | chord OPEN_PARENTHESIS INTEGER CLOSE_PARENTHESIS
chord         → CHORD | NO_CHORD
```

La gramática no tiene conflictos *shift/reduce* ni *reduce/reduce*.

## Decisiones de diseño

* **Sin punto y coma.** Se mantiene la sintaxis propuesta en el Stage I (`tempo 120`, `time 4/4`, `key C major`): cada construcción empieza con una palabra reservada y tiene una cantidad fija de elementos, por lo que no hace falta un separador.
* **Acorde como un único *token*.** Flex reconoce el acorde completo (`Cmaj7/E`) y lo descompone en raíz (`C`), cualidad (`maj7`) y bajo (`E`) dentro del valor semántico. La interpretación musical de la cualidad queda para el análisis semántico.
* **Contexto `KEY` en Flex.** Después de `key`, el analizador léxico solo acepta una nota y un modo; así `key C major` produce `NOTE` y no `CHORD`, y `key H major` o `key Cm major` se rechazan en el análisis léxico.
* **Identificadores en minúscula.** Como los acordes siempre empiezan con una nota en mayúscula (`A`–`G`), los identificadores (`verse`, `intro`, `piano`) empiezan en minúscula para que no haya ambigüedad.
* **Comentarios y metadatos.** Además de los comentarios `//` y `/* */`, las instrucciones `author` y `copyright` guardan en el AST información de autoría y licencia.
* **Reutilización con `pattern` / `use`.** Un `pattern` define un fragmento con nombre que después se reutiliza con `use nombre`, opcionalmente con repeticiones (`use intro x2`).
* **Instrumentos en paralelo con `track`.** Una sección puede contener compases directamente o una o más pistas (`track piano { ... }`). Las pistas de una misma sección suenan en paralelo. No se permite mezclar compases sueltos y pistas en la misma sección.
* **Doble barra.** `| C || G |` equivale a dos líneas de compases seguidas (`| C |` y `| G |`), igual que la doble barra en una partitura. Un compás vacío (`| |`) se rechaza.
* **Validaciones que quedan para el Stage III.** Que `xN` y las duraciones sean naturales positivos, que el denominador del compás sea potencia de dos, que los tiempos de cada compás sumen lo que indica `time`, que los patrones usados estén definidos, que el instrumento exista, etc. Estos casos están en `src/test/c/ignore/reject` y hoy se aceptan (falso positivo).

## Casos de prueba

| Carpeta | Contenido |
| :-- | :-- |
| `src/test/c/accept` | Programas que el compilador debe aceptar. |
| `src/test/c/reject` | Programas con errores léxicos o sintácticos que el compilador debe rechazar. |
| `src/test/c/ignore/reject` | Programas con errores semánticos. Se habilitarán en el Stage III. |

## Proyecto base: Flex-Bison-Compiler

Compilador construido sobre el proyecto base de la cátedra, desarrollado con Flex y Bison.

* [Requirements](#requirements)
* [Configuration](#configuration)
* [Commands](#commands)
* [CI/CD](#cicd)
* [Recommended Extensions](#recommended-extensions)

### Requirements

* [Docker v28.3.2](https://www.docker.com/)

### Configuration

Set the following environment variables to control and configure the behaviour of the application:

| Name                  | Default | Description                                                                                                                                                           |
| :-------------------- | :-----: | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ENVIRONMENT`         | `Local` | The active environment name. The available environments are: `Local`, `Development` and `Production`.                                                                 |
| `LOG_IGNORED_LEXEMES` | `true`  | When `true`, logs all of the ignored lexemes found with Flex at `DEBUGGING` level. To remove those logs from the console output set it to `false`.                    |
| `LOGGING_LEVEL`       | `ALL`   | The minimum level to log in the console output. From lower to higher, the available levels are: `ALL`, `DEBUGGING`, `INFORMATION`, `WARNING`, `ERROR` and `CRITICAL`. |

_Docker Compose_ can read the variables from an `.env` file too (see `compose.yaml` file).

### Commands

#### Start

Rises an ephemeral container, ready to start development:

```bash
docker compose run --rm compiler
```

#### Build

Builds or rebuilds the entire compiler:

```bash
src/main/bash/build.sh
```

#### Run

Compiles a program:

```bash
src/main/bash/run.sh <program>
```

where `<program>` is the path to the file that represents its entry-point.

#### Test

Executes every available unit-test under `src/test/c` folder:

```bash
src/main/bash/test.sh
```

#### Stop

Logout, destroy the ephemeral containers and shutdowns the cluster:

```bash
exit
docker compose down
```

#### Docker

| Command                                 | Description                                             |
| :-------------------------------------- | :------------------------------------------------------ |
| `docker builder prune --all`            | Removes all builds and complete build cache.            |
| `docker compose --progress=plain build` | Forces a build or rebuild of the images in the cluster. |
| `docker image prune`                    | Removes all of the dangling images from Docker.         |
| `docker network prune`                  | Removes unused networks from Docker.                    |
| `docker volume prune`                   | Removes unused volumes from Docker.                     |

### CI/CD

To trigger an automatic integration on every push or PR (_Pull Request_), you must activate _GitHub Actions_ in the _Settings_ tab. Use the following configuration:

| Key                                                        | Value                                               |
| :--------------------------------------------------------- | :-------------------------------------------------- |
| `Actions permissions`                                      | `Allow all actions and reusable workflows`          |
| `Allow GitHub Actions to create and approve pull requests` | `false`                                             |
| `Artifact and log retention`                               | `30 days`                                           |
| `Fork pull request workflows from outside collaborators`   | `Require approval for all outside collaborators`    |
| `Workflow permissions`                                     | `Read repository contents and packages permissions` |

### Recommended Extensions

* [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
* [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
* [Yash](https://marketplace.visualstudio.com/items?itemName=daohong-emilio.yash)
