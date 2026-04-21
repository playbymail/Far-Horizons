# Mining Difficulty

This document explains how mining difficulty works in Far Horizons — what
the number means, how it is set when planets are created, how home system
templates modify it, and how it changes during gameplay to affect your
colony's production. It is written for game designers and players.

---

## What Is Mining Difficulty?

Every planet has a **mining difficulty** (MD) rating that represents how
hard it is to extract raw materials from that world. A low MD means
resources are close to the surface, abundant, and easy to extract. A high
MD means mining is expensive, slow, or technically challenging.

Mining difficulty is stored as an integer equal to the "true" value
multiplied by 100. An MD of 250 means a difficulty of 2.50. This scaling
lets the engine use fast integer arithmetic while preserving two decimal
places of precision.

MD directly controls how many raw material units a colony produces each
turn and rises over time as resources are depleted.

---

## How Mining Difficulty Is Set at Planet Creation

### Galaxy Planets (Non-Home Worlds)

When the galaxy is first generated, every planet receives a mining
difficulty based on its **diameter** — larger planets tend to have higher
difficulty because their resources are spread across a bigger volume.

The formula works in two steps:

1. **Roll a base value.**
   Three six-sided-style dice are added and one four-sided die is
   subtracted, then the result is multiplied by a random factor of the
   planet's diameter and two small random offsets are added:

   ```
   md = (roll(1,3) + roll(1,3) + roll(1,3) - roll(1,4)) × roll(1, diameter)
        + roll(1,30) + roll(1,30)
   ```

   The result is re-rolled until it falls between **40 and 500**
   (inclusive), representing a raw difficulty range of 0.40 to 5.00.

2. **Apply a fudge factor.**
   The raw value is scaled up by 11/5 (multiplied by 11, then divided by
   5 with integer truncation). This pushes the effective range to
   roughly **88–1,100**, making most galaxy planets moderately difficult
   to very difficult to mine.

The fudge factor reflects the design intent that random planets in the
galaxy should generally be harder to mine than home worlds, giving
species an incentive to invest in Mining technology before expanding.

### Home System Template Planets (Non-Home Worlds)

When home system templates are generated, the non-home planets use a
wider and more forgiving formula:

```
md = (roll(1,3) + roll(1,3) + roll(1,3) - roll(1,4)) × roll(1, diameter)
     + roll(1,20) + roll(1,20)
```

The result is re-rolled until it falls between **30 and 1,000**. Crucially,
the 11/5 fudge factor is **not** applied. This produces a broader spread
of mining difficulties in a species' home system, from very easy (0.30)
to very hard (10.00), giving new players a mix of colonization targets
at different cost levels.

### The Home Planet Itself

The designated home planet (the earth-like world in every home system)
gets a **hardcoded mining difficulty** rather than a random one:

```
md = 208 + roll(1,11) + roll(1,11)
```

This yields a value between **210 and 230** (representing 2.10 to 2.30).
The narrow range ensures that every species starts with a roughly
equivalent economic foundation — no one gets an absurdly easy or
impossibly hard home world.

---

## How Home System Templates Modify Mining Difficulty

Templates are pre-generated sets of planet data that are applied to a
star system when a new species joins the game. During application, each
planet's mining difficulty is **randomly perturbed** so that no two home
systems are identical even when they share the same template.

The perturbation rule is:

- If the planet's MD is **greater than 100** (i.e., greater than 1.00),
  subtract a random amount between 1 and 10.
- Otherwise, if the MD is **greater than 0**, add a random amount between
  1 and 10.

This nudges high-difficulty planets slightly downward and low-difficulty
planets slightly upward, compressing them toward the center. The shift
is small — at most 0.10 on the displayed scale — so it preserves the
template's overall balance while giving each species a unique starting
position.

After perturbation, the modified MD is copied into the star system's
actual planet data. The planet's economic efficiency and MD increase
accumulator are both reset to zero.

---

## How Mining Difficulty Affects Production

Mining difficulty is one of the three factors that determine how many
**raw material units** (RMs) a colony produces each turn. The production
formula is:

```
raw_material_units = (10 × MI_tech_level × mining_base) / mining_difficulty
```

Where:

| Term               | Meaning                                                    |
|--------------------|------------------------------------------------------------|
| `MI_tech_level`    | The species' current Mining technology level                |
| `mining_base`      | The colony's installed mining infrastructure (MI base)     |
| `mining_difficulty`| The planet's current MD (stored as difficulty × 100)       |

Because MD is in the **denominator**, higher difficulty means fewer raw
materials. Doubling a planet's mining difficulty cuts its raw material
output in half (all else being equal). Conversely, advancing your Mining
tech level directly counteracts rising difficulty.

After this base calculation, two additional penalties may reduce the
output:

1. **Production penalty** — Colonies on planets requiring life support
   lose a percentage of output proportional to the life support needed
   and inversely proportional to the species' Life Support tech level.

2. **Economic efficiency** — Each planet has an efficiency rating
   (typically 100% for established colonies) that scales the output
   further.

The final number of raw material units is then compared to the colony's
**production capacity** (determined by Manufacturing tech and MA base).
The lesser of the two becomes the colony's **balance** — the economic
units available for spending that turn.

### Mining Colonies

Mining colonies are a special case. They exist solely to extract raw
materials and convert them into economic units for the species' treasury.
A mining colony converts two-thirds of its raw material output into
economic units (minus fleet maintenance costs). However, this extraction
comes at a cost: mining colonies cause the planet's mining difficulty to
**increase** each turn (see below).

---

## How Mining Difficulty Changes During Play

Mining difficulty is not static. It rises as colonies extract resources,
simulating resource depletion.

### Mining Colony Depletion

Each turn, every **mining colony** increases the planet's MD by an amount
proportional to how much it produced:

```
md_increase += raw_materials_produced / 150
```

This means a mining colony that produces 300 RMs in a turn will add 2
to the planet's MD (an increase of 0.02 on the displayed scale). A very
productive mining colony on a rich planet will deplete it faster than a
small operation.

Note that only **mining colonies** cause this increase. Regular colonies
and resort colonies do not deplete a planet's mineral resources, no
matter how much they produce.

### When the Increase Takes Effect

The MD increase does not apply immediately. Instead, the increase is
accumulated in a separate counter (`md_increase`) throughout the turn.
At the **end of the turn**, during the finish phase, the accumulated
increase is added to the planet's base mining difficulty and the counter
is reset to zero:

```
planet.mining_difficulty += planet.md_increase
planet.md_increase = 0
```

This means that all production calculations during a turn use the
**previous turn's** mining difficulty. The depletion from this turn's
mining shows up in next turn's numbers.

### Long-Term Implications

Because MD only goes up and never comes down, every mining colony has a
finite economic lifespan. As difficulty rises:

- Raw material output drops (MD is in the denominator of the production
  formula).
- The colony produces less, which means the MD increase per turn slows
  down (it is proportional to production).
- Eventually the colony reaches a point of diminishing returns where the
  cost of maintaining it exceeds the economic units it generates.

Players must decide when to abandon a depleted mining colony and
establish a new one on a fresh planet with lower mining difficulty.
Investing in Mining technology extends a colony's useful life by
increasing the numerator of the production formula, partially offsetting
the rising denominator.

---

## Summary

| Stage                     | What happens to MD                                      |
|---------------------------|---------------------------------------------------------|
| Galaxy creation           | Set from diameter-based formula with 11/5 fudge factor  |
| Home system template gen  | Set from wider formula (no fudge factor); home planet gets 210–230 |
| Template application      | Perturbed by ±1 to ±10 (pushes toward center)           |
| Each turn (mining colony) | Increases by `raw_materials_produced / 150`             |
| Turn end (finish phase)   | Accumulated increase applied; counter reset             |

Mining difficulty is the central economic "clock" of Far Horizons. It
starts low enough for early expansion, rises as players exploit
resources, and eventually forces strategic decisions about where to mine
next. Understanding MD helps players plan their economic development
and choose which planets are worth the investment.
