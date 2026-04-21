# Home System Templates Reference

This document describes the home system template subsystem: what templates
are, how they are generated, how they are stored, and how they are applied
to convert a star system into a home system. It is intended as a reference
for coding agents working on Far Horizons.

For planet generation internals, see [planet-generation.md](planet-generation.md).
For the full home system creation lifecycle (including species initialization),
see [home-system-generation.md](home-system-generation.md).
For LSN calculations, see [lsn-determination.md](lsn-determination.md).

## Overview

A home system template is a pre-generated set of planet data for a given
planet count. Templates are created once during game setup and reused each
time a new species needs a home system. Every system in the galaxy that
becomes a home system gets its planet data replaced by a copy of the
matching template (with minor random perturbations).

Templates exist for planet counts **3 through 9** (inclusive), producing
seven template files.

## Call Chain

```
CLI: fh create home-system-templates
  └─ createCommand()                          src/create.c:47
       └─ createHomeSystemTemplatesCommand()  src/create.c:179
            └─ createHomeSystemTemplates()    src/planet.c:34
                 └─ generate_planets()        src/planet.c:153  (called in a loop)
```

Later, when a species is created:

```
CLI: fh create species
  └─ createSpeciesCommand()                   src/create.c:247
       └─ changeSystemToHomeSystem()          src/star.c:48
            └─ getPlanetData()                src/planetio.h:27  (loads template)
```

## Key Source Files

| File             | Contents                                                |
|------------------|---------------------------------------------------------|
| `src/create.c`   | CLI command wrappers; `createHomeSystemTemplatesCommand` |
| `src/planet.c`   | `createHomeSystemTemplates`, `generate_planets`, `LSN`   |
| `src/planet.h`   | Function declarations; gas constant definitions          |
| `src/star.c`     | `changeSystemToHomeSystem` (template application)        |
| `src/engine.h`   | `planet_data` and `star_data` struct definitions         |
| `src/data.h`     | `binary_planet_data_t` (on-disk binary layout)           |
| `src/planetio.h` | Planet I/O functions (`savePlanetData`, `getPlanetData`)  |

## Data Structures

### `planet_data` (in-memory)

Defined in `src/engine.h:62–80`.

```c
struct planet_data {
    int id;                 // unique identifier
    int index;              // index into planet_base array
    int temperature_class;  // 1–30
    int pressure_class;     // 0–29
    int special;            // 0=none, 1=ideal home planet, 2=ideal colony, 3=radioactive
    int gas[4];             // atmospheric gases (0 = none)
    int gas_percent[4];     // percentage of each gas
    int diameter;           // thousands of km
    int gravity;            // Earth gravity × 100
    int mining_difficulty;  // mining difficulty × 100
    int econ_efficiency;    // always 100 for home planet
    int md_increase;        // mining difficulty increase per turn
    int message;            // message id (0 = none)
    int isValid;            // FALSE if record is invalid
    struct star_data *star; // parent star (not set in templates)
    int orbit;              // orbital position (not set in templates)
};
```

**Template-relevant fields:** Templates store `temperature_class`,
`pressure_class`, `special`, `gas[0..3]`, `gas_percent[0..3]`, `diameter`,
`gravity`, `mining_difficulty`. All other fields are zero in templates.

### `binary_planet_data_t` (on-disk)

Defined in `src/data.h:67–84`. Uses `uint8_t`, `int16_t`, and `int32_t`
for compact storage. The `savePlanetData` / `getPlanetData` functions
handle conversion between in-memory and binary formats.

### Gas Constants

Defined in `src/planet.h:31–43`.

| Constant | Value | Gas               |
|----------|-------|-------------------|
| `H2`     | 1     | Hydrogen          |
| `CH4`    | 2     | Methane           |
| `HE`     | 3     | Helium            |
| `NH3`    | 4     | Ammonia           |
| `N2`     | 5     | Nitrogen          |
| `CO2`    | 6     | Carbon Dioxide    |
| `O2`     | 7     | Oxygen            |
| `HCL`    | 8     | Hydrogen Chloride |
| `CL2`    | 9     | Chlorine          |
| `F2`     | 10    | Fluorine          |
| `H2O`    | 11    | Steam             |
| `SO2`    | 12    | Sulfur Dioxide    |
| `H2S`    | 13    | Hydrogen Sulfide  |

## Template Generation Algorithm

Entry point: `createHomeSystemTemplates()` in `src/planet.c:34–65`.

### Step 1 — Iterate Over Planet Counts

For each `num_planets` from 3 to 9:

1. Allocate an array of `num_planets` `planet_data` structs (zeroed).
2. Repeatedly call `generate_planets(planet_base, num_planets, earth_like=TRUE, makeMiningEasier=TRUE)` until the global flag `potential_home_system` is set to `TRUE`.
3. Save the template to `homesystem{n}.dat` (binary) and `homesystem{n}.txt` (S-expression).

### Step 2 — Planet Generation (per attempt)

`generate_planets()` at `src/planet.c:153–509` creates all planets in a
single call. It is called with `earth_like=TRUE` and `makeMiningEasier=TRUE`
for templates. See [planet-generation.md](planet-generation.md) for the
full algorithm. Key points for template generation:

- **Earth-like override:** The first planet with temperature class ≤ 11
  gets hardcoded earth-like values (diameter 12–14, gravity 97–120,
  temp class 10–12, pressure class 9–11, mining difficulty 210–230,
  `special=1`). Its atmosphere is composed of N₂ plus O₂ (11–30%),
  with optional NH₃ and CO₂ (1-in-3 chance each, up to 30%).
- **Easier mining:** Non-earth-like planets use a broader mining
  difficulty range (30–1000 × 100) without the 11/5 fudge factor.

### Step 3 — Viability Check

After `generate_planets` populates the array, it performs a viability
check (`src/planet.c:498–508`). The check only runs when a planet with
`special == 1` exists.

```
home_planet = planet with special == 1
potential = 0

for each planet in system:
    potential += 20000 / ((3 + LSN(planet, home_planet))
                          * (50 + planet.mining_difficulty))

system is viable only if: potential > 53 AND potential < 57
```

If the check fails, `potential_home_system` remains `FALSE` and the
outer loop in `createHomeSystemTemplates` re-generates from scratch.

### LSN Function

`LSN()` at `src/planet.c:515–552` computes the approximate Life Support
Needed between a candidate planet and the home planet:

```
ls_needed  = 2 * abs(candidate.temp_class - home.temp_class)
ls_needed += 2 * abs(candidate.pressure_class - home.pressure_class)
ls_needed += 2   // assume no oxygen
for each gas on candidate:
    if gas == O2: ls_needed -= 2
    if gas not on home planet: ls_needed += 2
```

## Output Files

For each planet count `n` (3–9), two files are written to the current
working directory:

| File                  | Format        | Writer                  |
|-----------------------|---------------|-------------------------|
| `homesystem{n}.dat`   | Binary        | `savePlanetData()`      |
| `homesystem{n}.txt`   | S-expression  | `planetDataAsSExpr()`   |

The `.dat` file is the authoritative template used at runtime. The `.txt`
file is a human-readable diagnostic dump.

Both files contain exactly `n` planet records with no star or orbit
metadata — those are assigned later when the template is applied.

## Template Application

When a species is created, `changeSystemToHomeSystem()` in
`src/star.c:48–135` loads and applies the matching template.

### Loading

```c
sprintf(filename, "homesystem%d.dat", star->num_planets);
planet_data_t *templateSystem = getPlanetData(0, filename);
```

### Randomization

Before copying, each planet in the template is randomly perturbed:

| Field               | Condition          | Adjustment                |
|---------------------|--------------------|---------------------------|
| `temperature_class` | > 12               | `−= rnd(3) − 1`          |
| `temperature_class` | > 0                | `+= rnd(3) − 1`          |
| `pressure_class`    | > 12               | `−= rnd(3) − 1`          |
| `pressure_class`    | > 0                | `+= rnd(3) − 1`          |
| `gas_percent[1,2]`  | `gas[2] > 0`       | Shift `rnd(25)+10` between slots 1 and 2 |
| `diameter`          | > 12               | `−= rnd(3) − 1`          |
| `diameter`          | > 0                | `+= rnd(3) − 1`          |
| `gravity`           | > 100              | `−= rnd(10)`             |
| `gravity`           | > 0                | `+= rnd(10)`             |
| `mining_difficulty`  | > 100             | `−= rnd(10)`             |
| `mining_difficulty`  | > 0               | `+= rnd(10)`             |

The `rnd(n)` function returns a uniformly distributed integer in `[1, n]`.
A `rnd(3) − 1` adjustment produces a change of 0, +1, or +2 (not
symmetric around zero).

### Copying

After randomization, these fields are copied from the (mutated) template
into the star system's actual planet data:

- `temperature_class`
- `pressure_class`
- `special`
- `gas[0..3]` and `gas_percent[0..3]`
- `diameter`
- `gravity`
- `mining_difficulty`
- `econ_efficiency`
- `md_increase`
- `message`
- `isValid`

The star is then marked: `star->home_system = TRUE`.

Fields **not** copied from the template (they retain existing values or
are set elsewhere): `id`, `index`, `star`, `orbit`.

## Important Invariants

1. **Templates are generated once** at game setup, before any species
   are created. The command is idempotent — rerunning it overwrites the
   previous templates.

2. **All home systems sharing a planet count use the same template.**
   Differentiation comes only from the randomization applied during
   `changeSystemToHomeSystem`.

3. **The viability window is narrow:** The potential score must be
   strictly between 53 and 57. This means many candidate sets are
   rejected; the generation loop may iterate hundreds or thousands of
   times for some planet counts.

4. **The global `potential_home_system` flag** (in `src/planet.c:31`)
   is the communication channel between `generate_planets` and
   `createHomeSystemTemplates`. It is set to `TRUE` only when the
   viability check passes.

5. **Template files contain no star/system metadata.** The planet
   records have zero-valued `id`, `index`, `star`, and `orbit` fields.
   These are populated when the template is applied to an actual system.

6. **The earth-like planet always has `special == 1`** in the template.
   This flag is preserved when copying to the target system and is
   later used to identify the home planet during species creation.

## Relationship to Other Subsystems

```
Galaxy Creation
  └─ creates star systems with random planets (earth_like=false)

Template Creation (this document)
  └─ creates homesystem{3..9}.dat files with earth_like=true planets

Species Creation
  ├─ selects a star system (Phase 2 in home-system-generation.md)
  ├─ calls changeSystemToHomeSystem() to apply template
  └─ initializes species gas tolerances, tech levels, colony
```
