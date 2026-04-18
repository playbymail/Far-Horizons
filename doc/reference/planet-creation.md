# Planet Creation Rules

This document specifies the rules for generating planets within a star system
in Far Horizons. It is intended as a reference for coding agents implementing
planet generation in Go.

For the overall galaxy creation process, see [galaxy-creation.md](galaxy-creation.md).

## Conventions

All arithmetic is **integer arithmetic** unless stated otherwise.
Integer division truncates toward zero.

`roll(low, high)` returns a uniformly distributed random integer
in the range `[low, high]` (inclusive on both ends).

When the document says "repeat until condition," it means a loop that
re-rolls every iteration until the condition is satisfied.

Constants, enumerations, and type definitions are in
[galaxy-creation.md](galaxy-creation.md).

## Inputs

```
num_planets       int    // number of planets to generate (1–9)
earth_like        bool   // if true, one planet will be made earth-like
makeMiningEasier  bool   // if true, use easier mining difficulty formula
```

During galaxy creation, `earth_like` and `makeMiningEasier` are both `false`.
They are `true` only when generating home system templates (outside the scope
of this document).

## Reference Tables

Seed values are based on Earth's solar system. Index 0 is unused.
Index 5 represents the asteroid belt (fantasy values).

```
start_diameter  = [_, 5, 12, 13,  7, 20, 143, 121, 51, 49]  // thousands of km
start_temp_class = [_, 29, 27, 11,  9,  8,   6,   5,  5,  3]
```

These tables are indexed by `baseValue` (computed per-planet below).

## Algorithm

Process planets sequentially from planet 1 to `num_planets`.
Earlier planets are closer to the star; later planets are farther away.

### Per-Planet Generation

For each `planet_number` from 1 to `num_planets`:

#### Step 1 — Compute Base Value

The base value selects a starting diameter and temperature class from the
reference tables.

```
if num_planets <= 3:
    baseValue = 2 * planet_number + 1
else:
    baseValue = (9 * planet_number) / num_planets
```

The `num_planets <= 3` case nudges values toward the Earth-like zone.

#### Step 2 — Starting Values

```
dia = start_diameter[baseValue]
tc  = start_temp_class[baseValue]
```

#### Step 3 — Randomize Diameter

```
die_size = dia / 4
if die_size < 2:
    die_size = 2

repeat 4 times:
    r = roll(1, die_size)
    if roll(1, 100) > 50:
        dia = dia + r
    else:
        dia = dia - r

// enforce minimum diameter of 3 (3,000 km)
while dia < 3:
    dia += roll(1, 4)

diameter = dia
```

> The maximum possible diameter is approximately 283 (283,000 km).

#### Step 4 — Determine Gas Giant Status

```
gas_giant = (diameter > 40)
```

Planets with diameter > 40,000 km are gas giants.

#### Step 5 — Compute Density

Density is scaled × 100 (so Earth ≈ 550).

```
if gas_giant:
    // range: 60 to 170
    density = 58 + roll(1, 56) + roll(1, 56)
else:
    // range: 370 to 570
    density = 368 + roll(1, 101) + roll(1, 101)
```

#### Step 6 — Compute Gravity

Gravity is scaled × 100 (so Earth = 100).

```
gravity = (density * diameter) / 72
```

> The divisor 72 is calibrated so that Earth's values (density=550,
> diameter=13) yield gravity=100.

#### Step 7 — Randomize Temperature Class

```
die_size = tc / 4
if die_size < 2:
    die_size = 2

n_rolls = roll(1, 3) + roll(1, 3) + roll(1, 3)

repeat n_rolls times:
    r = roll(1, die_size)
    if roll(1, 100) > 50:
        tc = tc + r
    else:
        tc = tc - r
```

Clamp by planet category:

```
if gas_giant:
    while tc < 3:
        tc += roll(1, 2)
    while tc > 7:
        tc -= roll(1, 2)
else:
    while tc < 1:
        tc += roll(1, 3)
    while tc > 30:
        tc -= roll(1, 3)
```

#### Step 8 — Warm Small Systems

If the system has fewer than 4 planets and this is planet 1 or 2, ensure
the planet is not too cold:

```
if num_planets < 4 AND planet_number < 3:
    while tc < 12:
        tc += roll(1, 4)
```

#### Step 9 — Enforce Temperature Ordering

Planets farther from the star must not be warmer than planets closer to it:

```
if planet_number > 1 AND temperature_class[planet_number - 1] < tc:
    tc = temperature_class[planet_number - 1]
```

#### Step 10 — Earth-Like Override

This step applies only when `earth_like` is `true` and it has not yet been
used. The override triggers on the **first** planet (in orbit order) whose
temperature class `tc <= 11`.

If triggered, replace all previously computed values for this planet:

```
diameter           = 11 + roll(1, 3)                        // 12–14
gravity            = 93 + roll(1, 11) + roll(1, 11) + roll(1, 5)  // 97–120
temperature_class  = 9 + roll(1, 3)                         // 10–12
pressure_class     = 8 + roll(1, 3)                         // 9–11
mining_difficulty   = 208 + roll(1, 11) + roll(1, 11)        // 210–230
special            = 1   // candidate ideal home planet
```

Generate atmosphere:

```
slot = 1
total_percent = 0

// 1-in-3 chance of ammonia
if roll(1, 3) == 1:
    pct = roll(1, 30)
    gas[slot] = NH3
    gas_percent[slot] = pct
    total_percent += pct
    slot++

nitro_slot = slot
slot++

// 1-in-3 chance of carbon dioxide
if roll(1, 3) == 1:
    pct = roll(1, 30)
    gas[slot] = CO2
    gas_percent[slot] = pct
    total_percent += pct
    slot++

// oxygen: 11–30%
pct = roll(1, 20) + 10
gas[slot] = O2
gas_percent[slot] = pct
total_percent += pct

// nitrogen gets the remainder
gas[nitro_slot] = N2
gas_percent[nitro_slot] = 100 - total_percent
```

Any unused gas slots (3rd and 4th) are set to gas=0, gas_percent=0.

After this override, **skip** the remaining steps (pressure class, atmosphere,
mining difficulty) for this planet and proceed to the next planet.

Mark `earth_like` as consumed so only one planet gets this treatment.

#### Step 11 — Compute Pressure Class (Non-Earth-Like Planets)

```
pc = gravity / 10
die_size = pc / 4
if die_size < 2:
    die_size = 2

n_rolls = roll(1, 3) + roll(1, 3) + roll(1, 3)

repeat n_rolls times:
    r = roll(1, die_size)
    if roll(1, 100) > 50:
        pc = pc + r
    else:
        pc = pc - r
```

Clamp by planet category:

```
if gas_giant:
    while pc < 11:
        pc += roll(1, 3)
    while pc > 29:
        pc -= roll(1, 3)
else:
    while pc < 0:
        pc += roll(1, 3)
    while pc > 12:
        pc -= roll(1, 3)
```

Override to zero (no atmosphere) if:

```
if gravity < 10:
    pc = 0    // gravity too low to retain atmosphere
else if tc < 2 OR tc > 27:
    pc = 0    // temperature too extreme for atmosphere
```

#### Step 12 — Generate Atmosphere

If `pressure_class == 0`, the planet has no atmosphere. Set all four gas
slots to gas=0, gas_percent=0.

Otherwise:

##### 12a — Determine Starting Gas Index

```
first_gas = (100 * tc) / 225
if first_gas < 1:
    first_gas = 1
else if first_gas > 9:
    first_gas = 9
```

This maps the temperature class to a starting position in the gas enumeration
(H2=1 through H2S=13). The planet will sample gases from `first_gas` through
`first_gas + 4` (five consecutive gas types).

##### 12b — Select Gases

```
num_gases_wanted = (roll(1, 4) + roll(1, 4)) / 2
num_gases_found = 0
gas_quantity = 0
```

Repeat the following until `num_gases_found > 0`:

For each gas index `i` from `first_gas` to `first_gas + 4`:

1. If `num_gases_found == num_gases_wanted`, stop iterating.
2. If `i == HE` (Helium, value 3):
   - Skip if `roll(1, 3) > 1` (2-in-3 chance of skipping).
   - Skip if `tc > 5` (too hot for helium).
   - Otherwise: add Helium with quantity = `roll(1, 20)`.
3. If `i != HE`:
   - If `roll(1, 3) == 3`, skip this gas.
   - Otherwise: add gas `i`.
     - If `i == O2`, quantity = `roll(1, 50)`.
     - Else, quantity = `roll(1, 100)`.
4. When a gas is added:
   ```
   num_gases_found++
   gas[num_gases_found] = gas_id
   gas_percent[num_gases_found] = quantity
   gas_quantity += quantity
   ```

> The outer "repeat until `num_gases_found > 0`" ensures at least one gas
> is always selected. If the inner loop finishes without finding any gas,
> restart the inner loop from `first_gas`.

##### 12c — Normalize to Percentages

```
total_percent = 0
for each found gas slot i (1 to num_gases_found):
    gas_percent[i] = (100 * gas_percent[i]) / gas_quantity
    total_percent += gas_percent[i]

// give any rounding remainder to the first gas
gas_percent[1] += 100 - total_percent
```

#### Step 13 — Compute Mining Difficulty

Mining difficulty is scaled × 100.

**Standard difficulty** (when `makeMiningEasier` is `false`):

```
mining_dif = 0
repeat until mining_dif >= 40 AND mining_dif <= 500:
    mining_dif = (roll(1,3) + roll(1,3) + roll(1,3) - roll(1,4))
                 * roll(1, diameter)
                 + roll(1, 30) + roll(1, 30)

// apply fudge factor
mining_dif = (mining_dif * 11) / 5
```

**Easier difficulty** (when `makeMiningEasier` is `true`):

```
mining_dif = 0
repeat until mining_dif >= 30 AND mining_dif <= 1000:
    mining_dif = (roll(1,3) + roll(1,3) + roll(1,3) - roll(1,4))
                 * roll(1, diameter)
                 + roll(1, 20) + roll(1, 20)
```

No fudge factor is applied in the easier case.

## Finalization

After all planets are generated, copy the per-planet working values into
`Planet` structs. Each planet gets:

```
planet.diameter           = diameter
planet.gravity            = gravity
planet.mining_difficulty  = mining_difficulty
planet.temperature_class  = temperature_class
planet.pressure_class     = pressure_class
planet.special            = special
planet.gas[0..3]          = gas[1..4]       // note: offset by 1
planet.gas_percent[0..3]  = gas_percent[1..4]
```

All other fields (`econ_efficiency`, `md_increase`, `message`) are
initialized to zero.

## Home System Viability Check

If any planet in the system has `special == 1` (candidate ideal home planet),
a viability score is computed across **all** planets in the system.

### LSN Function

The LSN (Life Support Needed) function computes the approximate environmental
difference between a candidate planet and the home planet:

```
func LSN(candidate, home_planet) -> int:
    ls_needed = 0

    // temperature difference
    tc_diff = abs(candidate.temperature_class - home_planet.temperature_class)
    ls_needed += 2 * tc_diff

    // pressure difference
    pc_diff = abs(candidate.pressure_class - home_planet.pressure_class)
    ls_needed += 2 * pc_diff

    // gas compatibility: assume oxygen is required
    ls_needed += 2   // start by assuming no oxygen

    for each gas g in candidate.gas[0..3]:
        if g == 0:
            continue
        if g == O2:
            ls_needed -= 2   // found oxygen
        poison = true
        for each gas k in home_planet.gas[0..3]:
            if g == k:
                poison = false
                break
        if poison:
            ls_needed += 2

    return ls_needed
```

### Viability Score

```
home_planet = the planet with special == 1
potential = 0

for each planet in system:
    potential += 20000 / ((3 + LSN(planet, home_planet)) * (50 + planet.mining_difficulty))
```

The system is a viable home system only if `potential > 53 AND potential < 57`.

If the viability check fails, clear the `special` flag on the candidate planet
(set `special = 0`) and mark the system as **not** a potential home system.

> **Note:** During galaxy creation, `earth_like` is `false`, so no planet will
> have `special == 1` and this check will never trigger. The viability check
> is relevant only when generating home system templates.
