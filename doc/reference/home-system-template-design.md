# Home System Template Design

This document describes the design for generating and applying home system
templates in the next-generation Far Horizons engine. It is intended for the
design team and is self-contained — you do not need to read other documents
to understand it.

## What Is a Home System Template?

Every player species in Far Horizons needs a home star system — a set of
planets where one planet is especially hospitable. Rather than generating
planets from scratch every time a new species joins the game, the engine
pre-builds a library of **templates**: one set of planet data for each
possible planet count (3 through 9). When a species is created, the engine
picks the template that matches the target star's planet count, applies
small random tweaks so no two home systems are identical, and overwrites
the star's planets with the result.

This gives every species a roughly equal start while still feeling unique.

### Lifecycle at a Glance

1. **Game setup** — The game master runs template generation once. It
   produces seven templates (for 3, 4, 5, 6, 7, 8, and 9 planets).

2. **Species creation** — Each time a new species is added, the engine
   selects a star system, loads the template matching its planet count,
   perturbs the template, and writes the modified data into the system's
   planets.

3. **Gameplay** — Templates are never used again after species creation.
   The planet data lives in the database from that point forward.

---

## Conventions

All arithmetic is **integer arithmetic**. Division truncates toward zero.

`roll(rng, low, high)` returns a uniformly distributed random integer in
`[low, high]` (inclusive on both ends), drawn from the provided `*rand.Rand`.

When this document says "repeat until condition," it means a loop that
regenerates from scratch on every iteration until the condition is met.

---

## Constants

### Gas Identifiers

Atmospheric gases are represented by integer identifiers:

| ID | Gas  | Name              |
|----|------|-------------------|
| 1  | H2   | Hydrogen          |
| 2  | CH4  | Methane           |
| 3  | HE   | Helium            |
| 4  | NH3  | Ammonia           |
| 5  | N2   | Nitrogen          |
| 6  | CO2  | Carbon Dioxide    |
| 7  | O2   | Oxygen            |
| 8  | HCL  | Hydrogen Chloride |
| 9  | CL2  | Chlorine          |
| 10 | F2   | Fluorine          |
| 11 | H2O  | Steam             |
| 12 | SO2  | Sulfur Dioxide    |
| 13 | H2S  | Hydrogen Sulfide  |

### Reference Tables

These seed values are loosely based on Earth's solar system. They are
indexed by a computed "base value" (see planet generation below). Index 0
is unused. Index 5 represents the asteroid belt (fantasy values).

```
START_DIAMETER   = [_, 5, 12, 13,  7, 20, 143, 121, 51, 49]   // thousands of km
START_TEMP_CLASS = [_, 29, 27, 11,  9,  8,   6,   5,  5,  3]
```

### Viability Window

A template is only accepted if its viability score falls **strictly**
between 53 and 57 (exclusive on both ends). This narrow window ensures
the system is neither too easy nor too hard for a starting species.

---

## Data Types

### Atmosphere

An atmosphere is a list of up to four gases, each with a percentage. The
percentages must sum to 100 (or the list is empty for vacuum worlds).

```
Atmosphere {
    Gases []AtmosphericGas   // 0 to 4 entries
}

AtmosphericGas {
    ID      int   // gas identifier (1–13); see table above
    Percent int   // percentage of atmosphere (1–100)
}
```

### TemplatePlanet

This is the data stored in a template. It represents one planet's physical
characteristics — everything needed to stamp a planet into a star system.

```
TemplatePlanet {
    Diameter         int         // thousands of km (minimum 3)
    Gravity          int         // surface gravity × 100 (Earth = 100)
    TemperatureClass int         // 1–30 (1 = cold, 30 = hot)
    PressureClass    int         // 0–29 (0 = vacuum)
    MiningDifficulty int         // mining difficulty × 100
    Atmosphere       Atmosphere  // atmospheric composition
    Special          int         // 0 = normal, 1 = ideal home planet
}
```

> **Why only these fields?** Templates capture the physical properties
> that define how habitable and economically useful a planet is. Metadata
> like planet ID, orbital position, and parent star are properties of the
> *system*, not the template, and are assigned when the template is applied.

### HomeSystemTemplate

The output of successful template generation.

```
HomeSystemTemplate {
    NumPlanets      int               // how many planets this template is for (3–9)
    Planets         []TemplatePlanet  // exactly NumPlanets entries, in orbit order
    ViabilityScore  int               // the score that passed the viability check (see Addendum A)
}
```

### GenerateInput

The input to template generation. The caller constructs this from whatever
data source it has (database, configuration, etc.).

```
GenerateInput {
    NumPlanets int   // number of planets to generate (3–9)
}
```

> The caller is responsible for calling the function repeatedly with
> different random states until a viable template is produced (the function
> returns null on failure). Alternatively, the caller can loop internally;
> the function itself does not loop — it makes one attempt and reports
> whether it succeeded.

### SystemPlanets

The mutable data passed to the template application function. It
represents the planets of a real star system that will be overwritten.

```
SystemPlanets {
    Planets []SystemPlanet   // the system's current planet data
}

SystemPlanet {
    Diameter         int
    Gravity          int
    TemperatureClass int
    PressureClass    int
    MiningDifficulty int
    EconEfficiency   int
    MdIncrease       int
    Atmosphere       Atmosphere
    Special          int
}
```

---

## Function 1 — Generate a Home System Template

```
func GenerateHomeSystemTemplate(rng *rand.Rand, input GenerateInput) *HomeSystemTemplate
```

**Returns** a `*HomeSystemTemplate` if the generated system passes the
viability check, or **null** if it does not.

The function has no side effects and accesses no global state. All
randomness comes from `rng`.

### Algorithm Overview

The function generates planets one at a time, from the innermost orbit
to the outermost. Planets closer to the star are hotter; planets farther
away are cooler. Exactly one planet is designated as the "earth-like"
candidate (the future home world). After all planets are generated, a
viability check determines whether the system as a whole is balanced
enough to serve as a home system.

### Step-by-Step

#### 1. Generate Each Planet

Process planets sequentially from orbit 1 to `input.NumPlanets`. Each
planet is generated independently except that temperature must not
increase as you move outward from the star.

##### 1a. Compute Starting Values

A "base value" maps the planet's orbital position into the reference
tables. For small systems (≤ 3 planets), the mapping is shifted to favor
Earth-like orbits:

```
if input.NumPlanets <= 3:
    baseValue = 2 * orbitNumber + 1
else:
    baseValue = (9 * orbitNumber) / input.NumPlanets

dia = START_DIAMETER[baseValue]
tc  = START_TEMP_CLASS[baseValue]
```

For example, in a 5-planet system, orbit 3 gets `baseValue = (9×3)/5 = 5`,
so `dia = 20` and `tc = 8`.

##### 1b. Randomize Diameter

The starting diameter is jittered with four random adjustments:

```
dieSize = max(dia / 4, 2)

repeat 4 times:
    r = roll(rng, 1, dieSize)
    if roll(rng, 1, 100) > 50:
        dia = dia + r
    else:
        dia = dia - r

// enforce minimum diameter
while dia < 3:
    dia = dia + roll(rng, 1, 4)
```

The minimum diameter is 3 (3,000 km). The theoretical maximum is about
283 (283,000 km).

##### 1c. Determine Gas Giant Status

```
isGasGiant = (dia > 40)
```

Planets wider than 40,000 km are gas giants. This affects density,
temperature limits, and pressure limits.

##### 1d. Compute Density and Gravity

Density is scaled × 100 (Earth ≈ 550):

```
if isGasGiant:
    density = 58 + roll(rng, 1, 56) + roll(rng, 1, 56)     // 60–170
else:
    density = 368 + roll(rng, 1, 101) + roll(rng, 1, 101)   // 370–570
```

Gravity is scaled × 100 (Earth = 100):

```
gravity = (density * dia) / 72
```

The divisor 72 is calibrated so that Earth's values (density 550,
diameter 13) produce gravity 100.

##### 1e. Randomize Temperature Class

```
dieSize = max(tc / 4, 2)
nRolls  = roll(rng, 1, 3) + roll(rng, 1, 3) + roll(rng, 1, 3)

repeat nRolls times:
    r = roll(rng, 1, dieSize)
    if roll(rng, 1, 100) > 50:
        tc = tc + r
    else:
        tc = tc - r
```

Then clamp to valid ranges:

```
if isGasGiant:
    clamp tc to [3, 7]      // bump up with roll(rng,1,2), trim with roll(rng,1,2)
else:
    clamp tc to [1, 30]     // bump up with roll(rng,1,3), trim with roll(rng,1,3)
```

Clamping uses random nudges rather than hard assignment: while the value
is out of range, add or subtract a small random amount until it's in
range. This avoids clustering at the boundary values.

##### 1f. Warm Small Systems

In systems with fewer than 4 planets, the first two orbits must not be
too cold — they are close to the star:

```
if input.NumPlanets < 4 AND orbitNumber <= 2:
    while tc < 12:
        tc = tc + roll(rng, 1, 4)
```

##### 1g. Enforce Temperature Ordering

A planet may not be warmer than the planet one orbit closer to the star:

```
if orbitNumber > 1 AND previousPlanetTC < tc:
    tc = previousPlanetTC
```

This ensures a monotonically non-increasing temperature gradient.

##### 1h. Earth-Like Override

This step fires **once** — on the first planet (in orbit order) whose
temperature class is ≤ 11. It replaces all previously computed values
for that planet with earth-like characteristics:

```
diameter         = 11 + roll(rng, 1, 3)                             // 12–14
gravity          = 93 + roll(rng, 1, 11) + roll(rng, 1, 11) + roll(rng, 1, 5)  // 97–120
temperatureClass = 9 + roll(rng, 1, 3)                              // 10–12
pressureClass    = 8 + roll(rng, 1, 3)                              // 9–11
miningDifficulty = 208 + roll(rng, 1, 11) + roll(rng, 1, 11)       // 210–230
special          = 1
```

The atmosphere is built from a specific recipe:

```
gases = []
totalPercent = 0

// 1-in-3 chance of ammonia
if roll(rng, 1, 3) == 1:
    pct = roll(rng, 1, 30)
    gases = append(gases, {ID: NH3, Percent: pct})
    totalPercent = totalPercent + pct

// reserve a slot for nitrogen (we'll fill in the percentage last)
nitroIndex = len(gases)
gases = append(gases, {ID: N2, Percent: 0})   // placeholder

// 1-in-3 chance of carbon dioxide
if roll(rng, 1, 3) == 1:
    pct = roll(rng, 1, 30)
    gases = append(gases, {ID: CO2, Percent: pct})
    totalPercent = totalPercent + pct

// oxygen: 11–30%
pct = roll(rng, 1, 20) + 10
gases = append(gases, {ID: O2, Percent: pct})
totalPercent = totalPercent + pct

// nitrogen gets whatever is left
gases[nitroIndex].Percent = 100 - totalPercent
```

After the earth-like override, **skip** steps 1i–1k for this planet
and proceed to the next orbit. Mark the override as consumed so it
does not fire again.

**Why does the override exist?** Without it, truly earth-like planets
are vanishingly rare in the random generation. The override guarantees
that every home system template has exactly one candidate home world
with breathable air, reasonable gravity, and moderate mining difficulty.

##### 1i. Compute Pressure Class (non-earth-like planets only)

```
pc = gravity / 10
dieSize = max(pc / 4, 2)
nRolls  = roll(rng, 1, 3) + roll(rng, 1, 3) + roll(rng, 1, 3)

repeat nRolls times:
    r = roll(rng, 1, dieSize)
    if roll(rng, 1, 100) > 50:
        pc = pc + r
    else:
        pc = pc - r
```

Clamp:

```
if isGasGiant:
    clamp pc to [11, 29]
else:
    clamp pc to [0, 12]
```

Force vacuum (pc = 0) if:

```
if gravity < 10:       // too small to hold an atmosphere
    pc = 0
else if tc < 2 OR tc > 27:  // too extreme for an atmosphere
    pc = 0
```

##### 1j. Generate Atmosphere (non-earth-like planets only)

If `pressureClass == 0`, the planet has no atmosphere (empty gas list).

Otherwise, determine which gases are available based on temperature:

```
firstGas = clamp((100 * tc) / 225, 1, 9)
```

This maps the temperature class to a starting position in the gas table.
The planet samples from five consecutive gas IDs: `firstGas` through
`firstGas + 4`.

Select gases:

```
numWanted = (roll(rng, 1, 4) + roll(rng, 1, 4)) / 2
gases     = []
totalQty  = 0

repeat until len(gases) > 0:
    for each gasID from firstGas to firstGas + 4:
        if len(gases) == numWanted:
            break

        if gasID == HE:
            if roll(rng, 1, 3) > 1:   continue   // 2-in-3 skip
            if tc > 5:                 continue   // too hot
            qty = roll(rng, 1, 20)
        else:
            if roll(rng, 1, 3) == 3:   continue   // 1-in-3 skip
            if gasID == O2:
                qty = roll(rng, 1, 50)             // oxygen is self-limiting
            else:
                qty = roll(rng, 1, 100)

        gases = append(gases, {ID: gasID, Percent: qty})
        totalQty = totalQty + qty
```

The outer "repeat until found" ensures at least one gas is always
selected. If all five candidates are skipped, try again.

Normalize quantities to percentages:

```
totalPercent = 0
for each gas in gases:
    gas.Percent = (100 * gas.Percent) / totalQty
    totalPercent = totalPercent + gas.Percent

// give rounding remainder to the first gas
gases[0].Percent = gases[0].Percent + (100 - totalPercent)
```

##### 1k. Compute Mining Difficulty (non-earth-like planets only)

Home system templates use the "easier" mining formula. The result is
scaled × 100:

```
miningDif = 0
repeat until miningDif >= 30 AND miningDif <= 1000:
    miningDif = (roll(rng,1,3) + roll(rng,1,3) + roll(rng,1,3) - roll(rng,1,4))
                * roll(rng, 1, dia)
                + roll(rng, 1, 20) + roll(rng, 1, 20)
```

No fudge factor is applied (unlike the standard formula used during
galaxy creation, which multiplies by 11/5).

#### 2. Viability Check

After all planets are generated, check whether exactly one planet has
`special == 1`. If none do, the attempt failed (return null). This would
only happen if no planet's temperature class reached ≤ 11 during
generation, which is rare but possible.

The planet with `special == 1` is the **home planet**. Compute a
viability score across the entire system:

```
homePlanet = the planet with special == 1
score = 0

for each planet in the system:
    lsn = approximateLSN(planet, homePlanet)
    score = score + 20000 / ((3 + lsn) * (50 + planet.miningDifficulty))
```

**Accept the template only if `score > 53 AND score < 57`.** Otherwise,
return null.

##### What the Viability Score Means

The score is a rough measure of how economically useful the system's
planets are *relative to the home planet*. Each planet contributes more
to the score when it has a low LSN (environmentally similar to the home
world) and low mining difficulty (easy to exploit).

The narrow acceptance window (54–56 inclusive) means:
- **Too low** → The non-home planets are too hostile or too hard to
  mine. The species would have a slow start.
- **Too high** → The system is too generous. The species would have
  an unfair advantage over species in less favorable systems.

In practice, many randomly generated planet sets fail this check. The
caller should expect to invoke the function tens to hundreds of times
before getting a viable template for a given planet count.

#### 3. Build and Return the Template

If the viability check passes, assemble a `HomeSystemTemplate`:

```
template = HomeSystemTemplate{
    NumPlanets:     input.NumPlanets,
    Planets:        [] of TemplatePlanet, one per generated planet,
    ViabilityScore: score,
}
return &template
```

---

## Approximate LSN Calculation

LSN ("Life Support Needed") estimates how much life support technology a
species would need to survive on a candidate planet, given what its home
planet looks like. Higher values mean the planet is more alien.

This is an *approximate* LSN used only during template generation. It
assumes the species breathes oxygen and treats any gas not present on the
home planet as poisonous.

```
func approximateLSN(candidate TemplatePlanet, home TemplatePlanet) int:
    lsn = 0

    // Temperature difference: each class of difference costs 2 points
    lsn = lsn + 2 * abs(candidate.TemperatureClass - home.TemperatureClass)

    // Pressure difference: each class of difference costs 2 points
    lsn = lsn + 2 * abs(candidate.PressureClass - home.PressureClass)

    // Gas compatibility
    // Start by assuming oxygen is absent (penalty of 2)
    lsn = lsn + 2

    homeGases = set of gas IDs on home planet

    for each gas in candidate.Atmosphere.Gases:
        if gas.ID == O2:
            lsn = lsn - 2          // found oxygen, remove the penalty

        if gas.ID not in homeGases:
            lsn = lsn + 2          // foreign gas is assumed poisonous

    return lsn
```

### Example

Suppose the home planet has atmosphere `[N2 70%, O2 20%, CO2 10%]` with
temperature class 11 and pressure class 10.

A neighboring planet has atmosphere `[N2 85%, HCL 15%]` with temperature
class 6 and pressure class 3.

```
lsn = 0
lsn += 2 * |6 - 11|   = 10     // temperature difference
lsn += 2 * |3 - 10|   = 14     // pressure difference
lsn += 2              =  2     // assume no oxygen

// N2 is on the home planet → not poison, no O2 bonus
// HCL is NOT on the home planet → poison, +2
lsn += 2              =  2

total lsn = 28
```

That planet's contribution to the viability score (with, say, mining
difficulty 150) would be: `20000 / ((3 + 28) * (50 + 150))` =
`20000 / (31 * 200)` = `20000 / 6200` = **3**.

Compare that with the home planet itself (lsn = 0, mining difficulty 220):
`20000 / ((3 + 0) * (50 + 220))` = `20000 / (3 * 270)` = `20000 / 810`
= **24**.

The home planet always dominates the score. The other planets add small
bonuses. The viability check ensures those bonuses land in the right range.

---

## Function 2 — Apply a Template to a System

```
func ApplyHomeSystemTemplate(
    rng      *rand.Rand,
    template *HomeSystemTemplate,
    system   *SystemPlanets,
) error
```

**Returns** null on success, or an error if the template cannot be applied
(for example, the planet counts do not match).

This function modifies `system.Planets` in place.

### Precondition

```
if len(template.Planets) != len(system.Planets):
    return error("template has %d planets but system has %d",
                 len(template.Planets), len(system.Planets))
```

### Algorithm

For each planet in the template, apply small random perturbations, then
copy the result into the corresponding system planet.

#### 1. Perturb the Template (Per Planet)

For each planet index `i` from 0 to `template.NumPlanets - 1`:

```
tp = copy of template.Planets[i]    // work on a copy to avoid mutating the template
```

##### Temperature Class

```
if tp.TemperatureClass > 12:
    tp.TemperatureClass = tp.TemperatureClass - (roll(rng, 1, 3) - 1)
else if tp.TemperatureClass > 0:
    tp.TemperatureClass = tp.TemperatureClass + (roll(rng, 1, 3) - 1)
```

The adjustment `roll(1,3) - 1` produces 0, 1, or 2. For hot planets
(class > 12) it tends to cool slightly; for others it tends to warm
slightly.

##### Pressure Class

```
if tp.PressureClass > 12:
    tp.PressureClass = tp.PressureClass - (roll(rng, 1, 3) - 1)
else if tp.PressureClass > 0:
    tp.PressureClass = tp.PressureClass + (roll(rng, 1, 3) - 1)
```

Same logic as temperature.

##### Atmosphere

If the planet has at least three gases in its atmosphere, shift percentage
between the second and third gas:

```
if len(tp.Atmosphere.Gases) >= 3:
    adjustment = roll(rng, 1, 25) + 10     // 11–35 percentage points

    if tp.Atmosphere.Gases[2].Percent > 50:
        tp.Atmosphere.Gases[1].Percent += adjustment
        tp.Atmosphere.Gases[2].Percent -= adjustment
    else if tp.Atmosphere.Gases[1].Percent > 50:
        tp.Atmosphere.Gases[1].Percent -= adjustment
        tp.Atmosphere.Gases[2].Percent += adjustment
```

This reshuffles the atmospheric composition without changing which gases
are present. The check for "> 50" ensures that percentages stay positive
in most cases (the dominant gas donates to the minor one).

##### Diameter

```
if tp.Diameter > 12:
    tp.Diameter = tp.Diameter - (roll(rng, 1, 3) - 1)
else if tp.Diameter > 0:
    tp.Diameter = tp.Diameter + (roll(rng, 1, 3) - 1)
```

##### Gravity

```
if tp.Gravity > 100:
    tp.Gravity = tp.Gravity - roll(rng, 1, 10)
else if tp.Gravity > 0:
    tp.Gravity = tp.Gravity + roll(rng, 1, 10)
```

Note this is `roll(1,10)`, not `roll(1,3)-1`, so gravity shifts are
slightly larger than diameter shifts.

##### Mining Difficulty

```
if tp.MiningDifficulty > 100:
    tp.MiningDifficulty = tp.MiningDifficulty - roll(rng, 1, 10)
else if tp.MiningDifficulty > 0:
    tp.MiningDifficulty = tp.MiningDifficulty + roll(rng, 1, 10)
```

#### 2. Copy Into the System

After perturbation, overwrite the system planet's physical properties:

```
sp = &system.Planets[i]

sp.TemperatureClass = tp.TemperatureClass
sp.PressureClass    = tp.PressureClass
sp.Special          = tp.Special
sp.Atmosphere       = tp.Atmosphere
sp.Diameter         = tp.Diameter
sp.Gravity          = tp.Gravity
sp.MiningDifficulty = tp.MiningDifficulty
sp.EconEfficiency   = 0
sp.MdIncrease       = 0
```

Fields that are **not** overwritten by the template (because they are
system-specific, not template-specific): planet ID, orbital position,
parent star reference, and any other metadata the system tracks.

#### 3. Return

```
return null   // success
```

### Example

Given a template planet with:
```
Diameter: 13,  Gravity: 105,  TemperatureClass: 11,  PressureClass: 10
MiningDifficulty: 218,  Atmosphere: [N2 62%, O2 23%, CO2 15%]
Special: 1
```

After perturbation (with specific rolls):
```
TemperatureClass: 11 + (roll→2 - 1) = 12
PressureClass:    10 + (roll→1 - 1) = 10   (unchanged)
Diameter:         13 + (roll→3 - 1) = 15
Gravity:          105 - roll→7      = 98
MiningDifficulty: 218 - roll→4      = 214
Atmosphere:       [N2 62%, O2 23%, CO2 15%]   (no shift: gas[2] is CO2 at 15%, ≤ 50; gas[1] is O2 at 23%, ≤ 50 → no change)
Special:          1
```

The resulting system planet is similar but not identical to the template.

---

## Design Rationale

### Why Templates Instead of Direct Generation?

Without templates, every home system would be generated independently.
The viability check's narrow window means most attempts fail — the
generation loop might run hundreds of times. Pre-generating templates
pays this cost once, at setup time, rather than every time a species
joins.

It also ensures that all species with the same planet count start from
the *same base template*. The random perturbations during application
create variety, but the overall balance (the viability score) was locked
in at setup time.

### Why Is the Viability Window So Narrow?

The window `(53, 57)` is exclusive on both ends, meaning only scores of
54, 55, or 56 are accepted. This was tuned empirically in the original
engine. A wider window would make template generation faster but would
increase variance between species' starting positions. The current window
represents a balance between generation speed and fairness.

### Why Does the Earth-Like Override Exist?

Without it, the random generation rarely produces a planet that is
simultaneously warm enough, dense enough, oxygen-bearing, and has
moderate mining difficulty. The override guarantees one such planet
per template while letting the remaining planets be fully random.

### Why Perturb During Application?

If every species with (say) a 5-planet home system got identical planet
data, players could trivially compare notes and discover they all started
the same. The perturbations are small enough to preserve the viability
balance but large enough to make each system feel unique.

---

## Summary of Functions

| Function                      | Input                           | Output                  | Side Effects |
|-------------------------------|---------------------------------|-------------------------|--------------|
| `GenerateHomeSystemTemplate`  | `rng`, `GenerateInput`          | `*HomeSystemTemplate`   | None         |
| `ApplyHomeSystemTemplate`     | `rng`, `*HomeSystemTemplate`, `*SystemPlanets` | `error`  | Mutates `system.Planets` |
| `approximateLSN`              | `candidate`, `home` (both `TemplatePlanet`) | `int`     | None         |

---

## Addendum A — The Viability Window as a Difficulty Knob

The viability window — the range of scores that `GenerateHomeSystemTemplate`
will accept — controls **how useful the non-home planets are for early
expansion**. It is a game-wide setting: all species created under the
same window get systems drawn from the same difficulty tier.

### Why the Viability Score is Returned

The `HomeSystemTemplate` struct includes the `ViabilityScore` that was
computed when the template was generated. This value should be persisted
alongside the template (in the database or template file) so that it can
be correlated with player feedback over time. Questions like "Do players
in games with score-55 templates report a better early-game experience
than those with score-54 templates?" become answerable when the score is
tracked.

### Anatomy of the Score

Recall the formula:

```
score = Σ  20000 / ((3 + LSN(planet, home)) * (50 + planet.miningDifficulty))
```

The home planet (LSN = 0, mining difficulty ~210–230) always contributes
roughly **23–25 points**. The remaining points come from the non-home
planets. So when we talk about shifting the window, we are really asking:
"How many points of expansion value should the neighboring planets
provide?"

| Window    | Neighbor contribution | Early-game feel                        |
|-----------|-----------------------|----------------------------------------|
| 46–50     | ~22–26 points         | Harsh — neighbors are hostile or barren |
| 54–56     | ~30–32 points         | Balanced — the original design          |
| 60–64     | ~36–40 points         | Generous — neighbors are inviting       |

### Shifting the Lower Bound

Raising the lower bound forces the generator to reject systems where the
neighbors are too hostile. The effect is a **floor on expansion quality**:
no species will start in a system where every neighbor requires heavy
life-support investment.

**Example:** Changing the window from `(53, 57)` to `(57, 61)` means
the weakest possible home system is now as good as the best possible home
system under the old window. A new player would find at least one or two
neighbors that are relatively cheap to colonize without advanced tech.

### Shifting the Upper Bound

Lowering the upper bound forces the generator to reject systems that are
too generous. The effect is a **ceiling on starting advantage**: no
species can luck into a cluster of easy, oxygen-rich, low-difficulty
neighbors.

**Example:** Changing the window from `(53, 57)` to `(53, 55)` tightens
the ceiling. Systems that would have scored 55 or 56 — the luckier end
of the old range — are now rejected. Every species starts in a slightly
tougher neighborhood.

### Shifting Both Bounds Together

Moving the entire window up or down without changing its width preserves
the fairness guarantee (all species are still within a 3-point band) while
uniformly making the game easier or harder.

| Adjustment         | Old window | New window | Effect                              |
|--------------------|------------|------------|-------------------------------------|
| Easier for all     | (53, 57)   | (58, 62)   | Better neighbors, faster expansion  |
| Harder for all     | (53, 57)   | (48, 52)   | Worse neighbors, slower expansion   |
| Tighter balance    | (53, 57)   | (54, 56)   | Less variance, slower generation    |
| Looser balance     | (53, 57)   | (50, 60)   | More variance, faster generation    |

### Widening the Window

A wider window (e.g., `(50, 60)`) accepts more planet sets, which speeds
up template generation dramatically. The trade-off is increased variance
between species. A species with score 51 lives in a noticeably harsher
neighborhood than one with score 59. Whether this variance is acceptable
depends on the game's goals:

- **Competitive games** should keep the window narrow (3–4 points wide)
  to minimize starting-position advantages.
- **Casual or narrative games** can afford a wider window (6–10 points)
  for faster setup and more diverse starting conditions.

### Generation Cost

The narrower the window, the more attempts the generator needs. Rough
empirical expectations from the original engine:

| Window width | Typical attempts per template |
|--------------|-------------------------------|
| 10 points    | ~10–50                        |
| 3 points     | ~100–500                      |
| 1 point      | ~500–5,000                    |

This cost is paid once at game setup (or once per species if generating
per-species templates), so even thousands of attempts complete in under
a second on modern hardware. It is not a practical constraint.

### Recommendation

Expose the lower and upper bounds as game configuration parameters
(e.g., `viability_min` and `viability_max`). Default them to 53 and 57
to preserve the original behavior. Track the `ViabilityScore` in the
database alongside each template so that it can be included in
post-game analytics and correlated with player satisfaction surveys.
