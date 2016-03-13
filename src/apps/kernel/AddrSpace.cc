/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
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

#include <m3/Common.h>
#include <m3/Config.h>

#include "AddrSpace.h"
#include "MainMemory.h"

namespace m3 {

AddrSpace::AddrSpace(int ep, capsel_t gate)
    : _ep(ep),
      _gate(gate),
      _rootpt(DTU::build_noc_addr(MEMORY_CORE, MainMemory::get().map().allocate(PAGE_SIZE))) {
}

AddrSpace::~AddrSpace() {
    MainMemory::get().map().free(DTU::noc_to_virt(_rootpt), PAGE_SIZE);
}

}