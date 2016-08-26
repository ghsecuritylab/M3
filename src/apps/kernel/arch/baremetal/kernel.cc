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

#include <base/stream/Serial.h>
#include <base/tracing/Tracing.h>
#include <base/log/Kernel.h>
#include <base/DTU.h>
#include <base/WorkLoop.h>

#include "mem/MainMemory.h"
#include "pes/PEManager.h"
#include "pes/VPEManager.h"
#include "Platform.h"
#include "SyscallHandler.h"

using namespace kernel;

int main(int argc, char *argv[]) {
    if(argc < 2) {
        m3::Serial::get() << "Usage: " << argv[0] << " <program>...\n";
        m3::Machine::shutdown();
    }

    EVENT_TRACE_INIT_KERNEL();

    KLOG(MEM, MainMemory::get());

    PEManager::create();
    VPEManager::create();
    VPEManager::get().load(argc - 1, argv + 1);

    PEManager::get().init();

    KLOG(INFO, "Kernel is ready");

    m3::env()->workloop()->run();

    EVENT_TRACE_FLUSH();

    KLOG(INFO, "Shutting down");

    VPEManager::destroy();

    m3::Machine::shutdown();
}
