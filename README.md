[![✗](https://img.shields.io/badge/Release-v2.0.0-ffb600.svg?style=for-the-badge)](https://github.com/vpark2003/TP-TLA/releases)

[![✗](https://github.com/vpark2003/TP-TLA/actions/workflows/pipeline.yaml/badge.svg?branch=development)](https://github.com/vpark2003/TP-TLA/actions/workflows/pipeline.yaml)

# B#

Lenguaje de dominio específico (DSL, *Domain Specific Language*) para describir la estructura armónica de canciones en formato *lead sheet*. Grupo **G-141**.

* [Uso](#uso)
* [Ejemplo](#ejemplo)
* [Gramática](#gramática)
* [Análisis semántico](#análisis-semántico)
* [Generación de código](#generación-de-código)
* [Decisiones de diseño](#decisiones-de-diseño)
* [Casos de prueba](#casos-de-prueba)

## Uso

El compilador lee un programa B# por entrada estándar, lo valida y genera un archivo MIDI (*Musical Instrument Digital Interface*). El primer argumento es la ruta del archivo de salida (por defecto, `output.mid`):

```bash
src/main/bash/build.sh
LOGGING_LEVEL=INFORMATION src/main/bash/run.sh src/test/c/accept/31-full-song-extended.bsharp song.mid
src/main/bash/test.sh
```

El código de salida es `0` si el programa es aceptado y `1` si se rechaza (por un error léxico, sintáctico o semántico). 

Fases del compilador:

1. **Análisis léxico** (`frontend/lexical-analysis`): Flex transforma el programa en un *stream* de *tokens*.
2. **Análisis sintáctico** (`frontend/syntactic-analysis`): Bison construye el AST (Árbol de Sintaxis Abstracta).
3. **Análisis semántico** (`backend/semantic-analysis`): valida el AST contra las reglas del dominio y construye la tabla de símbolos (patrones y secciones).
4. **Generación de código** (`backend/code-generation`): escribe un *Standard MIDI File*. El conocimiento musical compartido (notas, cualidades de acordes, instrumentos y tonalidades) está en `backend/domain`.

No se utilizan librerías externas: el archivo MIDI se escribe byte a byte.

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

## Análisis semántico

Valores por defecto: `tempo 120`, `time 4/4` y sin tonalidad. Las declaraciones globales pueden aparecer en cualquier orden, y un patrón puede usarse antes de definirse.

El análisis semántico (`backend/semantic-analysis`) registra los patrones y las secciones en una tabla de símbolos (`SymbolTable`), que después usa el generador para expandir cada `use`. Una sección puede repetir el nombre de otra (`verse`, `chorus`, `verse`). Cada error indica la sección, la pista y el número de compás donde ocurre.

**Errores (el programa se rechaza):**

| Regla | Ejemplo rechazado |
| :-- | :-- |
| `tempo`, `time`, `key`, `author` y `copyright` se declaran a lo sumo una vez. | `tempo 120` `tempo 90` |
| El tempo está entre 20 y 400 BPM (*Beats Per Minute*). | `tempo 0` |
| El numerador del compás está entre 1 y 32; el denominador es 1, 2, 4, 8, 16 o 32. | `time 4/3` |
| La tonalidad no puede ser teórica (más de 7 alteraciones). | `key G# major` |
| Las repeticiones `xN` de secciones y de `use` son naturales entre 1 y 100. | `section chorus x0` |
| Las duraciones explícitas son de al menos 1 tiempo. | `C(0)` |
| Los tiempos de cada compás suman exactamente el numerador del compás (ver abajo). | `\| C(3) G(2) \|` en 4/4 |
| La cualidad del acorde es una de las soportadas. | `Cmmm` |
| Los patrones tienen nombres únicos, los usados están definidos y no hay ciclos. | `pattern a { use b }` `pattern b { use a }` |
| Los instrumentos existen y no se repiten dentro de una sección. | `track kazoo { ... }` |
| Las pistas paralelas de una sección tienen la misma cantidad de compases (expandiendo patrones). | `track piano { \| C \| G \| }` `track bass { \| C \| }` |
| Hay a lo sumo 15 instrumentos distintos (canales MIDI, sin el de percusión). | — |
| La canción expandida tiene a lo sumo 10000 compases. | — |

**Duración de los acordes en un compás.** Los acordes con duración explícita (`C(2)`) ocupan esa cantidad de tiempos. Los tiempos restantes se reparten en partes iguales entre los acordes sin duración; si no se pueden repartir de forma exacta, es un error. Así, en 4/4, `| C |` equivale a `| C(4) |`, `| C G |` a `| C(2) G(2) |`, y `| Am F(2) G |` a `| Am(1) F(2) G(1) |`; en cambio, `| C G A |` se rechaza.

**Advertencias (el programa se acepta):** acordes no diatónicos (alguna de sus notas no pertenece a la escala de la tonalidad declarada; en tonalidades menores se acepta también la sensible, para que `E7` sea diatónico en `A minor`), patrones definidos y nunca usados, y canciones sin secciones.

**Cualidades soportadas:** mayor (vacía, `M`, `maj`), `m`, `min`, `dim`, `aug`, `+`, `sus`, `sus2`, `sus4`, `5`, `6`, `m6`, `7`, `maj7`, `M7`, `m7`, `min7`, `mmaj7`, `mM7`, `dim7`, `m7b5`, `aug7`, `+7`, `7#5`, `7b5`, `7sus4`, `add9`, `madd9`, `9`, `maj9`, `m9`, `7b9`, `7#9`, `11`, `m11`, `13`, `maj13` y `m13`.

**Instrumentos soportados:** `piano`, `epiano`, `harpsichord`, `vibraphone`, `marimba`, `organ`, `accordion`, `guitar`, `acoustic_guitar`, `electric_guitar`, `acoustic_bass`, `bass`, `violin`, `viola`, `cello`, `contrabass`, `harp`, `strings`, `choir`, `trumpet`, `trombone`, `brass`, `sax`, `clarinet`, `flute`, `synth` y `pad`.

## Generación de código

El compilador genera un *Standard MIDI File* de formato 1, con 480 *ticks* por negra:

* **Pista de director:** nombre de la canción, autor (evento de texto), *copyright*, tempo, compás y armadura de clave.
* **Una pista por instrumento**, cada una en su propio canal (se saltea el canal 10, reservado a percusión) y con su programa de General MIDI. Las secciones sin `track` usan `piano`.
* **Tempo:** el BPM se interpreta sobre la figura del denominador (en 6/8, `tempo 90` son 90 corcheas por minuto).
* **Acordes:** los instrumentos armónicos tocan todas las notas del acorde a partir de C3; si el acorde tiene bajo (`C/E`), el bajo se agrega una octava más abajo. Los instrumentos graves (`bass`, `acoustic_bass`, `contrabass`) tocan solo el bajo del acorde (la nota después de `/`, o la raíz), entre E1 y D#2.
* **`N.C.`** genera silencio durante su duración.
* **Secciones y patrones:** las secciones se concatenan en el orden en que aparecen, cada una repetida `xN` veces; los `use` se expanden en el lugar; las pistas de una misma sección empiezan en el mismo instante.

## Decisiones de diseño

* **Sin punto y coma.** Se mantiene la sintaxis propuesta en el Stage I (`tempo 120`, `time 4/4`, `key C major`): cada construcción empieza con una palabra reservada y tiene una cantidad fija de elementos, por lo que no hace falta un separador.
* **Acorde como un único *token*.** Flex reconoce el acorde completo (`Cmaj7/E`) y lo descompone en raíz (`C`), cualidad (`maj7`) y bajo (`E`) dentro del valor semántico. La interpretación musical de la cualidad queda para el análisis semántico.
* **Contexto `KEY` en Flex.** Después de `key`, el analizador léxico solo acepta una nota y un modo; así `key C major` produce `NOTE` y no `CHORD`, y `key H major` o `key Cm major` se rechazan en el análisis léxico.
* **Identificadores en minúscula.** Como los acordes siempre empiezan con una nota en mayúscula (`A`–`G`), los identificadores (`verse`, `intro`, `piano`) empiezan en minúscula para que no haya ambigüedad.
* **Comentarios y metadatos.** Además de los comentarios `//` y `/* */`, las instrucciones `author` y `copyright` guardan en el AST información de autoría y licencia.
* **Reutilización con `pattern` / `use`.** Un `pattern` define un fragmento con nombre que después se reutiliza con `use nombre`, opcionalmente con repeticiones (`use intro x2`).
* **Instrumentos en paralelo con `track`.** Una sección puede contener compases directamente o una o más pistas (`track piano { ... }`). Las pistas de una misma sección suenan en paralelo. No se permite mezclar compases sueltos y pistas en la misma sección.
* **Doble barra.** `| C || G |` equivale a dos líneas de compases seguidas (`| C |` y `| G |`), igual que la doble barra en una partitura. Un compás vacío (`| |`) se rechaza.
* **Validaciones en el análisis semántico.** La gramática acepta cualquier entero en `xN`, en las duraciones y en `time`, y cualquier combinación de letras como cualidad del acorde; los rangos válidos (los tipos "privados" como natural positivo o potencia de dos) se validan después del análisis sintáctico.

## Casos de prueba

| Carpeta | Contenido |
| :-- | :-- |
| `src/test/c/accept` | Programas que el compilador debe aceptar. |
| `src/test/c/reject` | Programas con errores léxicos o sintácticos (`01`–`29`) o semánticos (`30`–`50`) que el compilador debe rechazar. |

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
