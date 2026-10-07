# Lattice

Tracker génératif en plugin VST3 (instrument + sortie MIDI), pensé pour Ableton Live sous Windows.

## Compiler

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Lattice_VST3 Lattice_Standalone
```

JUCE 8.0.12 est récupéré automatiquement (ou utilisé depuis `libs/JUCE` s'il est présent).
Le plugin compilé se trouve dans `build/Lattice_artefacts/Release/VST3/Lattice.vst3` :
le copier dans `C:\Program Files\Common Files\VST3`.

Chaque push sur `main` compile Windows via GitHub Actions (artefact `Lattice-windows`).

## Arborescence

- `src/core` : modèle (song, patterns, cellules), séquenceur synchronisé sur l'hôte, générateurs, sauvegarde
- `src/synth` : voix « chip » (pulse, triangle, scie, bruit, ADSR, balayage, bitcrush, glide)
- `src/ui` : interface Graphite (grille, matrice, inspecteur, lane)
- `tests` : tests unitaires du moteur
- `tools/Shot.cpp` : rendu hors-ligne audio + capture de l'interface (Linux)

Polices : JetBrains Mono et Sora (SIL Open Font License, voir `resources/fonts/OFL.txt`).
