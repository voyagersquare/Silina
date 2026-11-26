# Silina 0.1.0

**Silina** je tape saturation plugin (VST3 + Standalone, Windows x64) by Voyagersquare.
Sučelje je kazeta: kontrole su na INDEX naljepnici na vrhu, koluti se okreću dok prolazi zvuk,
a narančasta strelica zažari kad traka ulazi u zasićenje.

![Silina](docs/preview.png)

## Kontrole

| Kontrola | Raspon | Što radi |
|---|---|---|
| **INPUT** | −12 … +24 dB | Koliko jako signal udara u traku. Više = više harmonika i mekše kompresije. |
| **BIAS** | 0 … 100 % | Nizak bias (under-bias): asimetrično, više parnih harmonika, svjetlije. Visok bias (over-bias): simetrično, glađe, tamnije. |
| **WOW** | 0 … 100 % | Sporo titranje visine tona (~0,55 Hz + nasumični drift). |
| **FLUTTER** | 0 … 100 % | Brzo titranje (6,3 Hz + 11,7 Hz). |
| **EMPHASIS** | 0 … 100 % | Pre-/de-emphasis (0–12 dB). Visoke frekvencije ranije ulaze u zasićenje, a ukupni odziv ostaje ravan. |
| **MIX** | 0 … 100 % | Dry/wet. Dry je poravnat s latencijom, pa nema faznih problema. |
| **OUTPUT** | −24 … +12 dB | Izlazna razina. |
| **TAPE SPEED** | 7.5 / 15 / 30 IPS | 7.5: toplije, veći head bump (~55 Hz), visoke padaju od ~11 kHz, više wow/fluttera. 30: čišće i otvorenije. |
| **AUTO GAIN** | uklj./isklj. | Izlaz se spušta za isto koliko je INPUT podignut — za pošteno A/B uspoređivanje. |

Dvoklik na kotačić vraća zadanu vrijednost. Prozor se može povećati/smanjiti (60–160 %).

## Obrada signala

```text
INPUT → pre-emphasis → 4x oversampling: magnetsko zasićenje (tanh, asimetrija i HF gubitak ovise o BIAS-u)
      → de-emphasis → head bump + low cut + HF roll-off (ovisno o brzini)
      → wow & flutter (modulirana linija kašnjenja) → AUTO GAIN → MIX (dry poravnat) → OUTPUT
```

Latencija je fiksna (oversampling FIR + osnovno kašnjenje transporta, ~2–3 ms) i javlja se DAW-u.

## Build

Windows: pokreni `Build-Windows.cmd` (Visual Studio 2022 s C++ i CMake alatima).
GitHub Actions gradi VST3 + Standalone, pokreće testove i pluginval (strictness 5) te objavljuje prijenosni paket.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Licenca

GPL-3.0. Koristi JUCE 8.0.6 (GPL) i VST3 SDK.
