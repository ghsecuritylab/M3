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

#include <base/Common.h>
#include <base/stream/IStringStream.h>
#include <base/CmdArgs.h>
#include <base/Errors.h>

#include <m3/session/M3FS.h>
#include <m3/session/ServerSession.h>
#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>

#include <limits>
#include <stdlib.h>

#include "sess/FileSession.h"
#include "sess/MetaSession.h"
#include "data/INodes.h"
#include "data/Dirs.h"
#include "FSHandle.h"

using namespace m3;

class M3FSRequestHandler;

using base_class = RequestHandler<
    M3FSRequestHandler, M3FS::Operation, M3FS::COUNT, M3FSSession
>;

static Server<M3FSRequestHandler> *srv;

class M3FSRequestHandler : public base_class {
public:
    explicit M3FSRequestHandler(size_t fssize, size_t extend, bool clear, bool revoke_first)
        : base_class(),
          _rgate(RecvGate::create(nextlog2<32 * M3FSSession::MSG_SIZE>::val,
                                  nextlog2<M3FSSession::MSG_SIZE>::val)),
          _mem(MemGate::create_global_for(FS_IMG_OFFSET, fssize, MemGate::RWX)),
          _handle(_mem.sel(), extend, clear, revoke_first) {
        add_operation(M3FS::READ, &M3FSRequestHandler::read);
        add_operation(M3FS::WRITE, &M3FSRequestHandler::write);
        add_operation(M3FS::FSTAT, &M3FSRequestHandler::fstat);
        add_operation(M3FS::SEEK, &M3FSRequestHandler::seek);
        add_operation(M3FS::STAT, &M3FSRequestHandler::stat);
        add_operation(M3FS::MKDIR, &M3FSRequestHandler::mkdir);
        add_operation(M3FS::RMDIR, &M3FSRequestHandler::rmdir);
        add_operation(M3FS::LINK, &M3FSRequestHandler::link);
        add_operation(M3FS::UNLINK, &M3FSRequestHandler::unlink);

        using std::placeholders::_1;
        _rgate.start(std::bind(&M3FSRequestHandler::handle_message, this, _1));
    }

    virtual Errors::Code open(M3FSSession **sess, capsel_t srv_sel, word_t) override {
        *sess = new M3FSMetaSession(srv_sel, _rgate, _handle);
        return Errors::NONE;
    }

    virtual Errors::Code obtain(M3FSSession *sess, KIF::Service::ExchangeData &data) override {
        if(sess->type() == M3FSSession::META) {
            auto meta = static_cast<M3FSMetaSession*>(sess);
            if(data.args.count == 0)
                return meta->get_sgate(data);
            return meta->open_file(srv->sel(), data);
        }
        else {
            auto file = static_cast<M3FSFileSession*>(sess);
            if(data.args.count == 0)
                return file->clone(srv->sel(), data);
            return file->get_mem(data);
        }
    }

    virtual Errors::Code delegate(M3FSSession *sess, KIF::Service::ExchangeData &data) override {
        if(sess->type() == M3FSSession::META || data.args.count != 0 || data.caps != 1)
            return Errors::NOT_SUP;

        capsel_t sel = VPE::self().alloc_sel();
        static_cast<M3FSFileSession*>(sess)->set_ep(sel);
        data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sel, data.caps).value();
        return Errors::NONE;
    }

    virtual Errors::Code close(M3FSSession *sess) override {
        delete sess;
        return Errors::NONE;
    }

    virtual void shutdown() override {
        _rgate.stop();
        _handle.flush_cache();
    }

    void read(GateIStream &is) {
        M3FSSession *sess = is.label<M3FSSession*>();
        sess->read(is);
    }

    void write(GateIStream &is) {
        M3FSSession *sess = is.label<M3FSSession*>();
        sess->write(is);
    }

    void seek(GateIStream &is) {
        M3FSSession *sess = is.label<M3FSSession*>();
        sess->seek(is);
    }

    void fstat(GateIStream &is) {
        M3FSSession *sess = is.label<M3FSSession*>();
        sess->fstat(is);
    }

    void stat(GateIStream &is) {
        M3FSSession *sess = is.label<M3FSSession*>();
        sess->stat(is);
    }

    void mkdir(GateIStream &is) {
        M3FSSession *sess = is.label<M3FSSession*>();
        sess->mkdir(is);
    }

    void rmdir(GateIStream &is) {
        M3FSSession *sess = is.label<M3FSSession*>();
        sess->rmdir(is);
    }

    void link(GateIStream &is) {
        M3FSSession *sess = is.label<M3FSSession*>();
        sess->link(is);
    }

    void unlink(GateIStream &is) {
        M3FSSession *sess = is.label<M3FSSession*>();
        sess->unlink(is);
    }

private:
    RecvGate _rgate;
    MemGate _mem;
    FSHandle _handle;
};

static void usage(const char *name) {
    Serial::get() << "Usage: " << name << " [-n <name>] [-e <blocks>] [-c] [-r] <size>\n";
    Serial::get() << "  -n: the name of the service (m3fs by default)\n";
    Serial::get() << "  -e: the number of blocks to extend files when appending\n";
    Serial::get() << "  -c: clear allocated blocks\n";
    Serial::get() << "  -r: revoke first, reply afterwards\n";
    exit(1);
}

int main(int argc, char *argv[]) {
    const char *name = "m3fs";
    size_t extend = 128;
    bool clear = false;
    bool revoke_first = false;

    int opt;
    while((opt = CmdArgs::get(argc, argv, "n:e:cr")) != -1) {
        switch(opt) {
            case 'n': name = CmdArgs::arg; break;
            case 'e': extend = IStringStream::read_from<size_t>(CmdArgs::arg); break;
            case 'c': clear = true; break;
            case 'r': revoke_first = true; break;
            default:
                usage(argv[0]);
        }
    }
    if(CmdArgs::ind >= argc)
        usage(argv[0]);

    size_t size = IStringStream::read_from<size_t>(argv[CmdArgs::ind]);
    srv = new Server<M3FSRequestHandler>(name, new M3FSRequestHandler(size, extend, clear, revoke_first));

    env()->workloop()->multithreaded(4);
    env()->workloop()->run();

    delete srv;
    return 0;
}
