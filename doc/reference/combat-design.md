# Combat Design

This document explains how combat works in Far Horizons — how battles
are initiated, how fighting strength is calculated, how rounds are
resolved, and what outcomes are possible. It is written for game
designers and players.

---

## Overview

Combat in Far Horizons occurs at a star system level. When one or more
species issue combat orders targeting the same sector (x, y, z), a
battle takes place. A battle can involve multiple species and can span
deep space, planetary orbits, and planetary surfaces within that sector.

Combat is divided into two separate phases each turn:

1. **Combat phase** — The main battle phase where all combat orders
   (BATTLE, ATTACK, ENGAGE, etc.) are resolved.
2. **Strike phase** — A secondary phase for hit-and-run attacks. Only
   engagement options 0–4 are valid during strikes.

---

## Initiating a Battle

### The BATTLE Command

A species initiates combat by issuing a `BATTLE x y z` command, which
declares the sector where fighting will take place. The species must
have a presence (ships or populated planets) in that sector.

Multiple species can issue BATTLE commands for the same sector. All
species present at a battle location are included in the battle — even
those who did not issue combat orders. Species that did not give combat
orders default to **Defense in Place** and can be taken by surprise.

### Declaring Enemies

After a BATTLE command, the `ATTACK` command names the target species.
A species can attack a specific enemy (`ATTACK SP <name>`) or all
declared enemies at once (`ATTACK 0`). The `HIJACK` command works like
ATTACK but attempts to capture ships for economic value instead of
destroying them.

When a species attacks another, the hostility is automatically made
mutual — the defender is forced to fight back. Additionally, if a
species attacks an ally of another species, that third species
automatically becomes an enemy of the attacker (the "auto-enemy"
mechanic).

---

## Engagement Options

After declaring a battle and enemies, a species uses the `ENGAGE`
command to specify how their forces will fight. Each engagement option
determines where and how ships participate.

| Option | Name                 | Description                                                     |
|--------|----------------------|-----------------------------------------------------------------|
| 0      | Defense in Place     | Ships defend at their current position (default).               |
| 1      | Deep Space Defense   | Ships defend in deep space, keeping the fight away from planets. |
| 2      | Planet Defense       | Ships defend a specific planet (requires planet number).        |
| 3      | Deep Space Fight     | Ships fight aggressively in deep space.                         |
| 4      | Planet Attack        | Ships attack a specific planet (requires planet number).        |
| 5      | Planet Bombardment   | Ships bombard a planet from orbit (requires planet number).     |
| 6      | Germ Warfare         | Ships deploy germ warfare bombs (requires planet number).       |
| 7      | Siege                | Ships besiege a planet (requires planet number).                |

Options 5–7 are not available during the strike phase.

A species may issue up to 20 engagement options per battle. The engine
processes these options sequentially, creating a series of actions within
the battle. Deep space fights are resolved first if any defender has
issued a Deep Space Defense order.

### Engagement Sequencing

1. If any species has ordered Deep Space Defense, a **deep space fight**
   is added as the first action.
2. All remaining engagement options are processed in order, creating a
   sequence of actions: deep space fights, planet attacks,
   bombardments, germ warfare, and sieges.
3. Between actions, shields are assumed to fully regenerate.

---

## Other Combat Orders

### WITHDRAW

`WITHDRAW <transport_age> <warship_age> <fleet_percentage>`

Sets conditions under which ships will jump away from the battle:

- **Transport withdraw age** — Transports that accumulate this much
  damage (age) will individually jump away. A value of 0 means
  transports only withdraw when the fleet does.
- **Warship withdraw age** — Warships that reach this damage level
  will individually jump away. Default is 100 (effectively never).
- **Fleet withdraw percentage** — When this percentage of the fleet
  has been destroyed or withdrawn, all remaining FTL ships jump away.
  A value of 0 means the entire fleet withdraws immediately. Default
  is 100 (fight to the last).

Ships that withdraw jump to the **haven** location.

### HAVEN

`HAVEN x y z`

Specifies the sector where withdrawing ships will jump to. If not
specified, a random nearby location is chosen.

### TARGET

`TARGET <type>`

Directs the species' fire toward a specific class of enemy unit:

| Value | Target type  |
|-------|-------------|
| 1     | Warships    |
| 2     | Transports  |
| 3     | Starbases   |
| 4     | Planetary defenses (PDs) |

When a target type is specified, there is a 75% chance each shot will
be directed at the preferred target (if available), with the remaining
25% of shots allocated normally.

### HIDE

`HIDE <ship>`

Marks a landed ship as a **non-combatant**. It will attempt to stay
out of the battle. However, if a species has *only* non-combatants
remaining, they are forced to fight.

### SUMMARY

`SUMMARY`

Requests an abbreviated combat report instead of a detailed
round-by-round log.

---

## Fighting Strength

A unit's effectiveness in combat is determined by its **offensive power**
and **defensive power**, both derived from a base **power value** that
depends on ship tonnage.

### The Power Function

Every ship class has a fixed tonnage (in units of 10,000 tons):

| Class | Abbr | Tonnage | Power   |
|-------|------|---------|---------|
| Picketboat       | PB | 1   | 100     |
| Corvette         | CT | 2   | 230     |
| Escort           | ES | 5   | 690     |
| Frigate          | FF | 10  | 1,585   |
| Destroyer        | DD | 15  | 2,578   |
| Light Cruiser    | CL | 20  | 3,641   |
| Strike Cruiser   | CS | 25  | 4,759   |
| Heavy Cruiser    | CA | 30  | 5,923   |
| Command Cruiser  | CC | 35  | 7,127   |
| Battlecruiser    | BC | 40  | 8,365   |
| Battleship       | BS | 45  | 9,635   |
| Dreadnought      | DN | 50  | 10,934  |
| Super Dreadnought | SD | 55 | 12,258  |
| Battlemoon       | BM | 60  | 13,608  |
| Battleworld      | BW | 65  | 14,979  |
| Battlestar       | BR | 70  | 16,373  |

Power scales sub-linearly — doubling tonnage does not double power.
This means that multiple smaller ships are more effective than a single
large ship of equal total tonnage.

**Transports** have their offensive and defensive power divided by 10,
reflecting that they are not designed for combat.

**Planetary defenses (PDs)** use an equivalent tonnage of
`PDs / 200` (minimum 1 if any PDs exist).

### Auxiliary Equipment

Warships (not transports or starbases) can carry auxiliary equipment
that enhances their combat capabilities:

- **Auxiliary Gun Units (GU1–GU9):** Each unit adds offensive power
  equal to `quantity × power(mark × 5)`, where mark is 1–9.
  A GU1 adds power(5) per unit; a GU9 adds power(45) per unit.
- **Auxiliary Shield Generators (SG1–SG9):** Each unit adds defensive
  power using the same formula. An SG1 adds power(5) per unit;
  an SG9 adds power(45) per unit.

### Age Degradation

Ships degrade with age. Both offensive and defensive power are reduced
by `(age / 50)` as a fraction. A brand-new ship (age 0) fights at full
strength. A ship at age 25 fights at half strength. A ship at age 50 or
above is considered destroyed.

```
effective_power = base_power - (age × base_power) / 50
```

### Technology Adjustments

After computing base power with equipment and age:

- **Offensive power** is increased by `(ML × offensive_power) / 50`,
  where ML is the species' Military tech level.
- **Defensive power** is increased by `(LS × defensive_power) / 50`,
  where LS is the species' Life Support tech level.

A species with ML 25 gets a 50% boost to offense. A species with LS 50
doubles its defensive power.

### Hijacking Penalty

If a species is attempting to hijack rather than destroy, both their
offensive and defensive power are divided by 4. Hijacking is inherently
risky.

### Shots and Damage

From the final offensive power, two values are derived:

- **Shots per round** = `(offensive_power / 1500) + 1`, capped at 5.
  If ML is 0 or offensive power is 0, shots are 0.
- **Damage per shot** = `(2 × offensive_power) / shots_per_round`.

**Shield strength** equals the final defensive power. Shields absorb
damage before it reaches the hull.

---

## Combat Resolution

### Round Structure

Each action within a battle is resolved in a series of rounds. The
maximum number of rounds depends on the action type:

| Action             | Maximum rounds |
|--------------------|----------------|
| Normal combat      | Unlimited (until one side is eliminated or withdraws) |
| Deep space fight (with defender advantage) | `defender_ML - attacker_ML` (minimum 1) |
| Planet bombardment | 10             |
| Germ warfare       | 1              |
| Siege              | 1              |

### Firing Sequence

Within each round, individual shots are resolved one at a time:

1. **Select attacker:** A random unit is chosen from all fighting units.
   Surprised units cannot fire. Non-combatants do not fire. Transports
   have only a 10% chance of firing each time they are selected.

2. **Select target:** Four random enemies are considered and the
   toughest one (highest `shots × damage`) is selected. Transports
   have only a 10% chance of being targeted unless the attacker has
   specifically targeted transports.

3. **Calculate hit chance:**
   ```
   chance_to_hit = (150 × attacker_ML) / (attacker_ML + defender_ML)
   ```
   - Doubled if defender is surprised.
   - Reduced by 25% if defender has a full complement of Field
     Distortion units.
   - Clamped to the range 2%–98%.
   - Reduced by `(2 × ship_age × chance) / 100` for aging ships.

4. **Calculate damage:** The base damage is the attacker's weapon
   damage, randomly adjusted by ±25%:
   ```
   damage = weapon_damage + ((26 - roll(1,51)) × weapon_damage) / 100
   ```

5. **Apply to shields:** If the defender's shields are up, a portion
   of the damage is absorbed. The split between shield and hull
   damage depends on the current shield percentage.

6. **Determine destruction:** If damage penetrates shields:
   - **Ships:** Age increases by `(50 × damage) / shield_strength / 2`
     (with random variation). If age reaches 50, the ship is destroyed.
   - **PDs:** A proportional number of planetary defense units are
     destroyed.

### Shield Regeneration

Between rounds, shields regenerate by `(5 + LS/10)%` of maximum
strength per round. High Life Support tech makes fleets more durable
across extended battles.

### Surprise

A species that did not issue combat orders and is the target of an
attack can be **taken by surprise**. Surprised units:

- Cannot fire during the first round.
- Have their hit chance doubled against them (effectively, shields are
  considered down).

After the first round of combat, the surprise effect ends.

### Ambush

A species can allocate economic units to an ambush using the
`use_on_ambush` setting on named planets. When combat begins, these
funds are used to set up the ambush against declared enemies.

---

## Special Combat Actions

### Planet Bombardment

Bombardment does not resolve as normal combat. Instead, ten rounds are
simulated to determine total bomb damage, which is then compared to a
reference value — the damage that ten Strike Cruisers at ML 50 would
deal in ten rounds:

```
reference_damage = 400 × power(25) = 1,903,600
```

The percent damage to the colony is:

```
percent = (total_bomb_damage × 250,000) / (reference_damage × total_population)
```

Where total population is `mi_base + ma_base` (plus 1 if colonist
units are present).

If percent exceeds 100, the colony is completely wiped out — all
mining base, manufacturing base, population, items, and shipyards are
destroyed. Ships under construction on the planet are also lost.

Otherwise, mining base, manufacturing base, population, items, and
shipyards are each reduced by the calculated percentage.

### Germ Warfare

Germ warfare uses GW (Germ Warfare Bomb) items. The chance of success
for each bomb is:

```
success_chance = 50 + 2 × (attacker_BI - defender_BI)
```

Where BI is the Biology tech level. Each bomb is rolled individually;
if any single bomb succeeds, the entire defending colony is wiped out.
Higher Biology tech on the defender's side reduces the chance; higher
Biology tech on the attacker's side increases it.

A successful germ warfare attack destroys everything on the colony —
population, infrastructure, items, and ships under construction. The
attacker also receives looting income equal to `mi_base + ma_base`
(×5 if the target is a home planet).

### Siege

A siege does not destroy anything directly. Instead, it marks the
targeted colony as **besieged**. Only ships that actually remain in the
system after all combat resolves will take part in the siege. A
besieged colony suffers economic penalties until the siege is lifted.

### Hijacking

When a ship is successfully "destroyed" by a hijacker, instead of
being lost, it generates economic units for the hijacker:

```
base_value = ship_cost × tonnage    (for transports and starbases)
base_value = ship_cost              (for warships)
```

Adjusted for sub-light ships (75% of cost) and ship age (a new ship
is worth more). The value of carried items is added on top. The total
is deposited into the hijacker's treasury.

---

## Forced Jump and Misjump Units

Starbases can use **Forced Jump (FJ)** or **Forced Misjump (FM)** units
to remove enemy ships from battle. The attacker must have at least as
many units as the target ship's tonnage, and the target cannot be
another starbase.

The chance of success is:

```
success_chance = 2 × ((units - target_tonnage) + (attacker_GV - defender_GV))
```

Where GV is the Gravitics tech level. If successful:

- **FJ:** The target is forced to jump to a random location near the
  battle (within ±2 parsecs).
- **FM:** The target is forced to a completely random location in the
  galaxy.

A species that emphasizes Gravitics technology over Military technology
will attempt to use FJ/FM units more often. The probability of
attempting this (instead of a normal shot) is:

```
fj_attempt_chance = 50 × attacker_GV / (attacker_GV + attacker_ML)
```

---

## Field Distortion

Ships carrying **Field Distortion (FD)** units can disguise their
identity. A fully loaded ship (FD count equals tonnage) reduces the
enemy's chance to hit by 25%. If combat damage destroys the FD units,
the ship's true name and owning species are revealed to all
participants.

---

## Withdrawal

After each round of combat (starting from the second round), the engine
checks whether ships should withdraw:

1. **Individual withdrawal:** If a ship's accumulated damage (age)
   exceeds its species' configured withdraw age, it jumps to the haven
   location. Transports and warships have separate thresholds.

2. **Fleet withdrawal:** If the percentage of ships lost or withdrawn
   exceeds the species' fleet withdrawal percentage, all remaining FTL
   ships jump away.

Only FTL ships can withdraw. Sub-light ships and starbases must fight
to the end.

---

## Destruction and Aftermath

### Ship Destruction

A ship is destroyed when its age reaches 50. After the battle, all
destroyed ships are permanently deleted. Items carried by a destroyed
ship are lost.

When a ship takes damage but survives, some of its carried items may
also be destroyed in proportion to the damage received.

### Colony Destruction

If bombardment or germ warfare reduces a colony's mining base,
manufacturing base, and population to zero, the colony is effectively
wiped out:

- All items are destroyed.
- All shipyards are destroyed.
- Ships under construction at that colony are deleted.
- The colony status is reset (it retains its home planet flag if
  applicable, but all infrastructure is gone).

### Looting

When a colony is destroyed, the attacker receives looting income as an
interspecies economic transfer. For home planets, the looting value is
multiplied by 5, reflecting the greater economic base.

---

## Summary

| Concept                  | Key formula or rule                                                |
|--------------------------|--------------------------------------------------------------------|
| Base power               | Looked up from tonnage table; scales sub-linearly                  |
| Offensive boost          | `+ML/50` fraction of base                                         |
| Defensive boost          | `+LS/50` fraction of base                                         |
| Age penalty              | `−age/50` fraction of power                                       |
| Shots per round          | `min(5, offensive_power / 1500 + 1)`                              |
| Hit chance               | `150 × attacker_ML / (attacker_ML + defender_ML)`, 2%–98%         |
| Shield regen per round   | `(5 + LS/10)%` of max                                             |
| Bombardment benchmark    | 10 Strike Cruisers at ML 50 for 10 rounds                         |
| Germ warfare success     | `50 + 2 × (attacker_BI − defender_BI)` per bomb                   |
| FJ/FM success            | `2 × (excess_units + GV_advantage)`                               |
| Transport combat penalty | Offensive and defensive power ÷ 10                                |
| Hijacking penalty        | Offensive and defensive power ÷ 4                                 |
| Ship destroyed           | Age ≥ 50                                                          |

Combat in Far Horizons rewards preparation, technology investment, and
tactical flexibility. Military tech directly improves hit rates and
offensive firepower, Life Support tech improves shields and shield
regeneration, Biology tech determines germ warfare outcomes, and
Gravitics tech enables forced-jump tactics. Fleet composition matters —
multiple smaller warships concentrate more total power than a single
large one, and transports are highly vulnerable in battle. Understanding
these mechanics helps players plan their military strategy and choose
when and where to fight.
