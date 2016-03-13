/**
 * Copyright (C) 2015, René Küttner <rene.kuettner@.tu-dresden.de>
 * Economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

namespace RCTMux {

void flag_set(const unsigned flag) {
    (*(volatile unsigned *)RCTMUX_FLAGS_LOCAL) |= flag;
}

void flag_unset(const unsigned flag) {
    (*(volatile unsigned *)RCTMUX_FLAGS_LOCAL) ^= flag;
}

void flags_reset() {
    (*(volatile unsigned *)RCTMUX_FLAGS_LOCAL) = 0;
}

bool flag_is_set(const unsigned flag) {
    return ((*(volatile unsigned *)RCTMUX_FLAGS_LOCAL) & flag);
}

} /* namespace RCTMux */