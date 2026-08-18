# CHIP-8 Emulator

Un interprete/emulatore CHIP-8 scritto in C++, con rendering tramite SFML.

CHIP-8 è un linguaggio interpretato nato negli anni '70 per far girare videogiochi su piccoli computer amatoriali dell'epoca. Questo progetto ricrea da zero un interprete moderno in grado di caricare ed eseguire ROM `.ch8` storiche (Pong, Tetris, Space Invaders, Breakout e molte altre).

## Caratteristiche

- Ciclo fetch-decode-execute completo, con supporto a tutte delle istruzioni standard CHIP-8
- Stack, registri, timer (delay/sound) e memoria a 4KB implementati fedelmente alla spec originale
- Rendering dello schermo 64x32 tramite SFML
- Font esadecimale integrato per la visualizzazione di cifre/punteggi
- Input mappato su tastiera QWERTY, che replica il layout del tastierino esadecimale originale
- Timer eseguiti su un thread separato, sincronizzati a 60Hz

## Requisiti

- Compilatore C++17 o superiore
- [SFML](https://www.sfml-dev.org/) (Graphics, Window) versione precompilata 2.6.2
- CMake 3.x+

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```
> nota: è necessario modificare il path assoluto della risorsa sfml nel cmakelists per poter compilare con cmake.

## Utilizzo

Avvia l'eseguibile e inserisci il path di una ROM `.ch8` quando richiesto.

### Mappatura tasti

Il tastierino esadecimale originale del CHIP-8:

```
1 2 3 C        1 2 3 4
4 5 6 D   →    Q W E R
7 8 9 E        A S D F
A 0 B F        Z X C V
```

## Stato del progetto

Progetto personale sviluppato come esercizio di apprendimento su emulazione, gestione della memoria a basso livello e integrazione con SFML. In sviluppo attivo.

## Licenza

MIT
