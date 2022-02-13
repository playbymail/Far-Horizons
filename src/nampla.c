// Far Horizons Game Engine
// Copyright (C) 2022 Michael D Henderson
// Copyright (C) 2021 Raven Zachary
// Copyright (C) 2019 Casey Link, Adam Piggott
// Copyright (C) 1999 Richard A. Morneau
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <stdio.h>
#include "engine.h"
#include "nampla.h"
#include "log.h"

struct nampla_data *namp_data[MAX_SPECIES];
struct nampla_data *nampla_base;
struct nampla_data *nampla;

// Additional memory must be allocated for routines that name planets.
// This is the default 'extras', which may be changed, if necessary.
int extra_namplas = NUM_EXTRA_NAMPLAS;
int num_new_namplas[MAX_SPECIES];

/* This routine will set or clear the POPULATED bit for a nampla.
 * It will return TRUE if the nampla is populated or FALSE if not.
 * It will also check if a message associated with this planet should be logged. */
int check_population(struct nampla_data *nampla) {
    int is_now_populated, was_already_populated;
    long total_pop;
    char filename[32];

    if (nampla->status & POPULATED) {
        was_already_populated = TRUE;
    } else {
        was_already_populated = FALSE;
    }
    total_pop = nampla->mi_base + nampla->ma_base + nampla->IUs_to_install + nampla->AUs_to_install +
                nampla->item_quantity[PD] + nampla->item_quantity[CU] + nampla->pop_units;
    if (total_pop > 0) {
        nampla->status |= POPULATED;
        is_now_populated = TRUE;
    } else {
        nampla->status &= ~(POPULATED | MINING_COLONY | RESORT_COLONY);
        is_now_populated = FALSE;
    }
    if (is_now_populated && !was_already_populated) {
        if (nampla->message) {
            /* There is a message that must be logged whenever this planet becomes populated for the first time. */
            sprintf(filename, "message%ld.txt\0", nampla->message);
            log_message(filename);
        }
    }
    return is_now_populated;
}

