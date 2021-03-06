#
# Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
# Economic rights: Technische Universitaet Dresden (Germany)
#
# This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
#
# M3 is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# M3 is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License version 2 for more details.
#

#include <base/Asm.h>

BEGIN_FUNC(gem5_shutdown)
    .long   0xEE210110

1:  wfi
    b       1b
END_FUNC(gem5_shutdown)

BEGIN_FUNC(gem5_writefile)
    .long   0xEE4F0110
    mov     pc, lr
END_FUNC(gem5_writefile)

BEGIN_FUNC(gem5_readfile)
    .long   0xEE500110
    mov     pc, lr
END_FUNC(gem5_readfile)

BEGIN_FUNC(gem5_resetstats)
    .long   0xEE400110
    mov     pc, lr
END_FUNC(gem5_resetstats)

BEGIN_FUNC(gem5_dumpstats)
    .long   0xEE410110
    mov     pc, lr
END_FUNC(gem5_dumpstats)
