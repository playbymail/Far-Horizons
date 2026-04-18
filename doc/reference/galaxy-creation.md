# Galaxy Creation Rules

This document specifies the rules for creating a Far Horizons galaxy.
It is intended as a reference for coding agents implementing galaxy creation in Go.

Planet generation is detailed in [planet-creation.md](planet-creation.md).

## Conventions

All arithmetic is **integer arithmetic** unless stated otherwise.
Integer division truncates toward zero.

`roll(low, high)` returns a uniformly distributed random integer
in the range `[low, high]` (inclusive on both ends).

When the document says "repeat until condition," it means a loop that
re-rolls every iteration until the condition is satisfied.

## Constants

```
STANDARD_NUMBER_OF_SPECIES      = 15
STANDARD_NUMBER_OF_STAR_SYSTEMS = 90
STANDARD_GALACTIC_RADIUS        = 20   // parsecs

MIN_SPECIES =   1
MAX_SPECIES = 100

MIN_STARS   =   12
MAX_STARS   = 1000

MIN_RADIUS  =  6
MAX_RADIUS  = 50

MAX_DIAMETER = 2 * MAX_RADIUS   // 100
MAX_PLANETS  = 9 * MAX_STARS    // 9000
```

## Enumerations

### Star Type

| Name          | Value |
|---------------|-------|
| DWARF         | 1     |
| DEGENERATE    | 2     |
| MAIN_SEQUENCE | 3     |
| GIANT         | 4     |

### Star Color

| Name         | Value |
|--------------|-------|
| BLUE         | 1     |
| BLUE_WHITE   | 2     |
| WHITE        | 3     |
| YELLOW_WHITE | 4     |
| YELLOW       | 5     |
| ORANGE       | 6     |
| RED          | 7     |

### Atmospheric Gas

| Name | Value | Description        |
|------|-------|--------------------|
| H2   | 1     | Hydrogen           |
| CH4  | 2     | Methane            |
| HE   | 3     | Helium             |
| NH3  | 4     | Ammonia            |
| N2   | 5     | Nitrogen           |
| CO2  | 6     | Carbon Dioxide     |
| O2   | 7     | Oxygen             |
| HCL  | 8     | Hydrogen Chloride  |
| CL2  | 9     | Chlorine           |
| F2   | 10    | Fluorine           |
| H2O  | 11    | Steam              |
| SO2  | 12    | Sulfur Dioxide     |
| H2S  | 13    | Hydrogen Sulfide   |

### Planet Special

| Value | Meaning                |
|-------|------------------------|
| 0     | Not special            |
| 1     | Ideal home planet      |
| 2     | Ideal colony planet    |
| 3     | Radioactive hell-hole  |

## Type Definitions

### Galaxy

```
Galaxy {
    d_num_species  int   // design (maximum) number of species
    num_species    int   // actual number of species allocated (0 at creation)
    radius         int   // galactic radius in parsecs
    turn_number    int   // current turn number (0 at creation)
}
```

### Star

```
Star {
    x              int        // coordinate (0-based, within the cube of side galactic_diameter)
    y              int
    z              int
    type           StarType   // DWARF | DEGENERATE | MAIN_SEQUENCE | GIANT
    color          StarColor  // BLUE through RED
    size           int        // 0 through 9 inclusive
    num_planets    int        // 1 through 9 inclusive
    home_system    bool       // true if this is a designated home system
    worm_here      bool       // true if a wormhole entry/exit exists here
    worm_x         int        // coordinates of the wormhole's other end
    worm_y         int        //   (only meaningful when worm_here is true)
    worm_z         int
    planet_index   int        // index of the first planet in the global planets array
    message        int        // message id (0 = none)
    visited_by     []uint32   // bit-set; bit N is set if species N has visited
    planets        []Planet   // the planets orbiting this star
}
```

### Planet

```
Planet {
    temperature_class  int      // 1–30
    pressure_class     int      // 0–29
    special            int      // see Planet Special enum
    gas                [4]int   // gas id for each atmospheric slot (0 = none)
    gas_percent        [4]int   // percentage for each atmospheric slot
    diameter           int      // in thousands of kilometers
    gravity            int      // surface gravity × 100 (100 = 1.0 Earth g)
    mining_difficulty  int      // mining difficulty × 100
    econ_efficiency    int      // economic efficiency (always 100 for home planet)
    md_increase        int      // increase in mining difficulty per turn
    message            int      // message id (0 = none)
}
```

## Inputs

Galaxy creation requires three inputs:

| Input                | Description                                      |
|----------------------|--------------------------------------------------|
| `desiredNumSpecies`  | Number of species the galaxy is designed for.     |
| `desiredNumStars`    | Number of star systems to generate.               |
| `galacticRadius`     | Radius of the galaxy in parsecs.                  |

### Deriving Defaults

If `desiredNumStars` is not provided, compute it:

```
desiredNumStars = desiredNumSpecies * STANDARD_NUMBER_OF_STAR_SYSTEMS
                  / STANDARD_NUMBER_OF_SPECIES
```

For a "less crowded" galaxy, increase by 50%:

```
desiredNumStars = 3 * desiredNumSpecies * STANDARD_NUMBER_OF_STAR_SYSTEMS
                  / (2 * STANDARD_NUMBER_OF_SPECIES)
```

If `galacticRadius` is not provided, compute the minimum volume needed and
find the smallest integer radius whose cube is ≥ that volume:

```
minVolume = desiredNumStars
            * STANDARD_GALACTIC_RADIUS * STANDARD_GALACTIC_RADIUS * STANDARD_GALACTIC_RADIUS
            / STANDARD_NUMBER_OF_STAR_SYSTEMS

galacticRadius = MIN_RADIUS
while galacticRadius^3 < minVolume:
    galacticRadius++
```

### Validation

Reject if any of these are true:

- `desiredNumSpecies` < `MIN_SPECIES` or > `MAX_SPECIES`
- `desiredNumStars` < `MIN_STARS` or > `MAX_STARS`
- `galacticRadius` < `MIN_RADIUS` or > `MAX_RADIUS`

## Galaxy Creation Algorithm

### Step 1 — Compute Derived Values

```
galactic_diameter = 2 * galacticRadius
galactic_volume   = (4 * 314 * galacticRadius * galacticRadius * galacticRadius) / 300
chance_of_star    = galactic_volume / desiredNumStars
```

Reject if `chance_of_star` < 50 (radius too small) or > 3200 (radius too large).

> **Note on volume formula:** The expression `(4 * 314 * r³) / 300` is an
> integer approximation of the sphere volume `(4/3) × π × r³`. The constants
> 314 and 300 approximate π ≈ 3.14 with `314/100`, then `4 × 314 / 300`
> simplifies the combined `4/3 × 3.14` factor.

### Step 2 — Initialize Location Grid

Create a 2D array `star_here[MAX_DIAMETER][MAX_DIAMETER]` of integers.
Initialize every cell to `-1` (meaning "no star at this x,y").

> This grid records the z-coordinate of a star at a given (x, y).
> Only one star can exist at each (x, y) coordinate pair.

### Step 3 — Place Stars

Place exactly `desiredNumStars` stars. For each star to place:

1. Loop until a valid placement is found:
   a. Pick random coordinates:
      ```
      x = roll(1, galactic_diameter) - 1
      y = roll(1, galactic_diameter) - 1
      z = roll(1, galactic_diameter) - 1
      ```
   b. Convert to real (centered) coordinates:
      ```
      real_x = x - galacticRadius
      real_y = y - galacticRadius
      real_z = z - galacticRadius
      ```
   c. Check that the point is inside the galactic sphere:
      ```
      sq_distance = real_x² + real_y² + real_z²
      ```
      If `sq_distance >= galacticRadius²`, reject and retry.
   d. Check that no star already exists at `(x, y)`:
      If `star_here[x][y] != -1`, reject and retry.
   e. Record the star: `star_here[x][y] = z`.

### Step 4 — Initialize Galaxy Record

```
galaxy.d_num_species = desiredNumSpecies
galaxy.num_species   = 0
galaxy.radius        = galacticRadius
galaxy.turn_number   = 0
```

### Step 5 — Generate Star and Planet Data

Iterate over the grid in row-major order (x from 0 to galactic_diameter−1,
y from 0 to galactic_diameter−1). For each `(x, y)` where
`star_here[x][y] != -1`:

#### 5a — Set Coordinates

```
star.x = x
star.y = y
star.z = star_here[x][y]
```

#### 5b — Determine Star Type

```
star_type = roll(1, GIANT + 6)    // roll(1, 10)
if star_type > GIANT:             // if star_type > 4
    star_type = MAIN_SEQUENCE     //   star_type = 3
```

This makes MAIN_SEQUENCE the most common type (probability 7/10).

#### 5c — Determine Star Color

```
star_color = roll(1, RED)         // roll(1, 7)
```

Uniformly distributed across all seven colors.

#### 5d — Determine Star Size

```
star_size = roll(1, 10) - 1       // 0 through 9
```

#### 5e — Determine Number of Planets

The number of planets depends on star color and star type.

```
d = RED + 2 - star_color          // die size: 9 - star_color + 2 = (9 - color)
                                  // bluer stars → bigger die
```

Determine the number of rolls based on star type:

```
num_rolls = star_type
if num_rolls > 2:
    num_rolls -= 1
```

This gives:
- DWARF (1) → 1 roll
- DEGENERATE (2) → 2 rolls
- MAIN_SEQUENCE (3) → 2 rolls
- GIANT (4) → 3 rolls

Roll for the planet count:

```
num_planets = -2
for i in 1..num_rolls:
    num_planets += roll(1, d)
```

Clamp the result:

```
while num_planets > 9:
    num_planets -= roll(1, 3)
if num_planets < 1:
    num_planets = 1
```

#### 5f — Generate Planets

Call the planet generation procedure (see [planet-creation.md](planet-creation.md))
with `earth_like = false` and `makeMiningEasier = false`.

Record `planet_index` as the current position in the global planets array
before generating planets.

#### 5g — Set Remaining Fields

```
star.home_system = false
star.worm_here   = false
```

### Step 6 — Generate Wormholes

Iterate over all stars (index `i` from 0 to `desiredNumStars - 1`).
For each star:

1. Roll `roll(1, 100)`. If the result is **< 92**, skip this star (no wormhole).
   Only values **≥ 92** (i.e., 92..100, a 9% chance) proceed.
2. If the star is already a home system or already has a wormhole, skip.
3. Find the other endpoint:
   - Loop until a valid endpoint is found:
     a. Pick a random star index: `j = roll(1, desiredNumStars) - 1`.
     b. The endpoint is valid if it is not the same star AND it is not a home
        system AND it does not already have a wormhole.
4. Compute the squared distance between the two stars:
   ```
   dx = star[i].x - star[j].x
   dy = star[i].y - star[j].y
   dz = star[i].z - star[j].z
   sq_dist = dx² + dy² + dz²
   ```
5. If `sq_dist < 400` (less than 20 parsecs apart), discard this wormhole
   (skip to next star). Do **not** retry.
6. Otherwise, link the two stars bidirectionally:
   ```
   star[i].worm_here = true
   star[i].worm_x = star[j].x
   star[i].worm_y = star[j].y
   star[i].worm_z = star[j].z

   star[j].worm_here = true
   star[j].worm_x = star[i].x
   star[j].worm_y = star[i].y
   star[j].worm_z = star[i].z
   ```

### Step 7 — Output

The result of galaxy creation is three data sets:

1. **Galaxy data** — A single `Galaxy` record.
2. **Star data** — An array of `desiredNumStars` `Star` records.
3. **Planet data** — An array of all `Planet` records (total count =
   sum of `num_planets` across all stars).

## Boundary Note

Galaxy creation does **not** assign home systems to species. Home system
assignment occurs in a separate step (species creation) that is outside
the scope of this document.
