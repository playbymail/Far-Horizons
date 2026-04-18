# Life Support Needed (LSN) Determination

This document specifies the rules for computing the Life Support Needed
(LSN) value for a planet relative to a species' home planet. It is
intended as a reference for coding agents implementing LSN in Go.

Constants, enumerations, and type definitions are in
[galaxy-creation.md](galaxy-creation.md).

## Overview

LSN measures how much life support technology a species needs to survive
on a given planet. A value of 0 means the planet is naturally habitable
for the species. Higher values mean more life support infrastructure is
required.

There are two variants of the LSN calculation:

1. **Full LSN** — Used during gameplay. Accounts for species-specific
   required gas, gas percentage tolerances, and poison gases.
2. **Approximate LSN** — Used only during galaxy creation (home system
   viability check). Assumes oxygen is required and any gas not present
   on the home planet is poisonous. Uses a smaller multiplier.

## Inputs

### Full LSN

```
species  Species   // the species whose tolerances are being checked
home     Planet    // the species' home planet
colony   Planet    // the planet being evaluated
```

### Approximate LSN

```
home     Planet    // the home planet
colony   Planet    // the planet being evaluated
```

No species data is used in the approximate variant.

## Relevant Type Fields

### Planet

```
temperature_class  int      // 1–30
pressure_class     int      // 0–29
gas                [4]int   // gas id for each atmospheric slot (0 = none)
gas_percent        [4]int   // percentage for each atmospheric slot
```

### Species

```
required_gas       int      // gas id the species must breathe
required_gas_min   int      // minimum acceptable percentage of required gas
required_gas_max   int      // maximum acceptable percentage of required gas
neutral_gas        [6]int   // gases that are harmless to the species
poison_gas         [6]int   // gases that are toxic to the species
```

## Full LSN Algorithm

This is the authoritative LSN calculation used during gameplay for
production penalties, colony viability, terraforming, resort colony
eligibility, and scan reports.

Each environmental difference contributes **3 points** of life support
needed.

### Step 1 — Temperature Difference

```
tc_diff = abs(colony.temperature_class - home.temperature_class)
```

### Step 2 — Pressure Difference

```
pc_diff = abs(colony.pressure_class - home.pressure_class)
```

### Step 3 — Check Atmosphere

Scan the colony planet's atmosphere for the species' required gas and
for poison gases.

```
has_required_gas = false
poison_count = 0

for each gas slot j in colony.gas[0..3]:
    if colony.gas_percent[j] == 0:
        continue

    if colony.gas[j] == species.required_gas:
        if species.required_gas_min <= colony.gas_percent[j]
           AND colony.gas_percent[j] <= species.required_gas_max:
            has_required_gas = true
    else:
        for each poison gas p in species.poison_gas[0..5]:
            if colony.gas[j] == p:
                poison_count++
                break
```

Notes:
- A gas is only checked against the poison list if it is **not** the
  required gas.
- The required gas must be present **and** within the acceptable
  percentage range to count.
- Neutral gases (gases in the species' `neutral_gas` list) are not
  explicitly checked. A gas that is neither required nor poisonous
  contributes nothing to LSN.

### Step 4 — Compute LSN

```
ls_needed = 3 * (tc_diff + pc_diff + poison_count)

if has_required_gas == false:
    ls_needed += 3
```

### Step 5 — Return

Return `ls_needed`. A value of 0 means the colony is naturally
habitable for this species.

## Approximate LSN Algorithm

This simplified variant is used **only** during galaxy creation for the
home system viability check (see [planet-creation.md](planet-creation.md)).
It does not use species data.

Each environmental difference contributes **2 points** (not 3).

### Assumptions

- The required gas is oxygen (`O2`, value 7).
- Any gas present on the colony planet that does **not** appear on the
  home planet is considered poisonous.

### Algorithm

```
tc_diff = abs(colony.temperature_class - home.temperature_class)
ls_needed = 2 * tc_diff

pc_diff = abs(colony.pressure_class - home.pressure_class)
ls_needed += 2 * pc_diff

// assume oxygen is not present
ls_needed += 2

for each gas slot j in colony.gas[0..3]:
    if colony.gas[j] == 0:
        continue

    if colony.gas[j] == O2:
        ls_needed -= 2

    poison = true
    for each gas slot k in home.gas[0..3]:
        if colony.gas[j] == home.gas[k]:
            poison = false
            break

    if poison:
        ls_needed += 2

return ls_needed
```

## Usage Context

LSN is used in several gameplay systems:

### Production Penalty

Colonies on planets with LSN > 0 suffer a production penalty:

```
if ls_needed == 0:
    production_penalty = 0
else:
    production_penalty = (100 * ls_needed) / species.tech_level[LS]
```

The penalty is applied as a percentage reduction to raw material
production and manufacturing capacity. If the penalty reaches or
exceeds 100% (i.e., `ls_needed >= tech_level[LS]`), the colony is
destroyed.

### Colony Growth

Population growth rate on a colony is:

```
percent_increase = 10 * (100 - (100 * ls_needed) / ls_actual) / 100
```

where `ls_actual` is the species' Life Support tech level. If this
value is negative, the colony is wiped out.

### Resort Colony Eligibility

A colony qualifies as a resort colony only if `ls_needed <= 6` (among
other conditions).

### Terraforming

Terraforming plants (TPs) reduce LSN by modifying the colony planet's
atmosphere, temperature, and pressure to match the home planet. Each
terraforming action consumes 3 TPs and addresses one source of LSN
(removing a poison gas, adjusting the required gas, or moving
temperature/pressure one step closer to the home planet's value).

The priority order for terraforming changes is:
1. Remove a poison gas from the atmosphere.
2. Add or adjust the required gas to the correct percentage range.
3. Move temperature class one step toward the home planet's value.
4. Move pressure class one step toward the home planet's value.
