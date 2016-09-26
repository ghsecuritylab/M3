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

#pragma once

#include <base/col/SList.h>
#include <base/KIF.h>

#include <thread/ThreadManager.h>

#include <cstring>

#include "com/RecvBufs.h"
#include "cap/CapTable.h"
#include "cap/Capability.h"
#include "mem/AddrSpace.h"
#include "mem/SlabCache.h"
#include "pes/VPEDesc.h"
#include "DTUState.h"
#include "Types.h"

namespace kernel {

class ContextSwitcher;
class PEManager;
class VPECapability;
class VPEManager;

class VPE : public m3::SListItem, public SlabObject<VPE> {
    friend class ContextSwitcher;
    friend class PEManager;
    friend class VPECapability;
    friend class VPEManager;

    struct ServName : public m3::SListItem {
        explicit ServName(const m3::String &_name) : name(_name) {
        }
        m3::String name;
    };

public:
    static const uint16_t INVALID_ID    = 0xFFFF;

    static const cycles_t TIME_SLICE    = 1000000;

    static const int SYSC_MSGSIZE_ORD   = m3::nextlog2<512>::val;
    static const int SYSC_CREDIT_ORD    = SYSC_MSGSIZE_ORD;
    static const int NOTIFY_MSGSIZE_ORD = m3::nextlog2<64>::val;

    enum State {
        RUNNING,
        SUSPENDED,
        DEAD
    };

    enum Flags {
        F_BOOTMOD     = 1 << 0,
        F_DAEMON      = 1 << 1,
        F_IDLE        = 1 << 2,
        F_INIT        = 1 << 3,
        F_HASAPP      = 1 << 4,
        F_MUXABLE     = 1 << 5, // TODO temporary
        F_READY       = 1 << 6,
        F_WAITING     = 1 << 7,
    };

    explicit VPE(m3::String &&prog, peid_t peid, vpeid_t id, uint flags, epid_t ep = -1,
        capsel_t pfgate = m3::KIF::INV_SEL);
    VPE(const VPE &) = delete;
    VPE &operator=(const VPE &) = delete;
    ~VPE();

    const VPEDesc &desc() const {
        return _desc;
    }
    vpeid_t id() const {
        return desc().id;
    }
    int pid() const {
        return _pid;
    }
    peid_t pe() const {
        return desc().pe;
    }
    const m3::String &name() const {
        return _name;
    }

    void set_pe(peid_t pe) {
        _desc.pe = pe;
    }

    uint flags() const {
        return _flags;
    }
    int state() const {
        return _state;
    }

    AddrSpace *address_space() {
        return _as;
    }
    RecvBufs &rbufs() {
        return _rbufs;
    }

    int exitcode() const {
        return _exitcode;
    }

    bool is_waiting() const {
        return _flags & F_WAITING;
    }
    void start_wait() {
        assert(!(_flags & F_WAITING));
        _flags |= F_WAITING;
    }
    void stop_wait() {
        assert(_flags & F_WAITING);
        _flags &= ~F_WAITING;
    }

    void subscribe_exit(const m3::Subscriptions<int>::callback_type &cb) {
        _exitsubscr.subscribe(cb);
    }
    void unsubscribe_exit(m3::Subscriber<int> *sub) {
        _exitsubscr.unsubscribe(sub);
    }
    void subscribe_resume(const m3::Subscriptions<bool>::callback_type &cb) {
        _resumesubscr.subscribe(cb);
    }
    void unsubscribe_resume(m3::Subscriber<bool> *sub) {
        _resumesubscr.unsubscribe(sub);
    }

    const m3::SList<ServName> &requirements() const {
        return _requires;
    }
    void add_requirement(const m3::String &name) {
        _requires.append(new ServName(name));
    }

    CapTable &objcaps() {
        return _objcaps;
    }
    CapTable &mapcaps() {
        return _mapcaps;
    }

    RecvGate &syscall_gate() {
        return _syscgate;
    }
    SendGate &upcall_sgate() {
        return _upcsgate;
    }

    SendQueue &upcall_queue() {
        return _upcqueue;
    }

    void upcall(const void *msg, size_t size, bool onheap) {
        _upcqueue.send(&_upcsgate, msg, size, onheap);
    }

    bool has_app() const {
        return _flags & F_HASAPP;
    }

    void start_app();
    void stop_app();
    void exit_app(int exitcode);

    void yield();

    bool migrate();
    bool resume(bool need_app = true, bool unblock = true);
    void wakeup();

    void make_daemon();

    m3::Errors::Code xchg_ep(size_t epid, MsgCapability *oldcapobj, MsgCapability *newcapobj);

    void invalidate_ep(epid_t ep);

    void config_snd_ep(epid_t ep, label_t lbl, peid_t pe, vpeid_t vpe, epid_t dstep, size_t msgsize, word_t crd);
    void config_rcv_ep(epid_t ep, uintptr_t buf, uint order, uint msgorder, int flags);
    void config_mem_ep(epid_t ep, peid_t dstpe, vpeid_t dstvpe, uintptr_t addr, size_t size, int perm);

private:
    int refcount() const {
        return _refs;
    }
    void ref() {
        _refs++;
    }
    void unref();

    void init();
    void init_memory();
    void load_app(const char *name);

    void update_ep(epid_t ep);

    void notify_resume();

    void write_env_file(int pid, label_t label, epid_t epid);
    void activate_sysc_ep(void *addr);

    void free_reqs();

    VPEDesc _desc;
    uint _flags;
    int _refs;
    int _pid;
    int _state;
    int _exitcode;
    m3::String _name;
    CapTable _objcaps;
    CapTable _mapcaps;
    uint64_t _lastsched;
    alignas(DTU_PKG_SIZE) DTUState _dtustate;
    RecvGate _syscgate;
    SendGate _upcsgate;
    SendQueue _upcqueue;
    RecvBufs _rbufs;
    AddrSpace *_as;
    m3::SList<ServName> _requires;
    m3::Subscriptions<int> _exitsubscr;
    m3::Subscriptions<bool> _resumesubscr;
};

}
