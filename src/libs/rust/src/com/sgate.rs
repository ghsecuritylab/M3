use cap::{Flags, Selector};
use com::gate::Gate;
use com::RecvGate;
use dtu;
use dtu::EpId;
use errors::Error;
use kif::INVALID_SEL;
use syscalls;
use vpe;

#[derive(Debug)]
pub struct SendGate {
    gate: Gate,
}

pub struct SGateArgs {
    rgate_sel: Selector,
    label: dtu::Label,
    credits: u64,
    sel: Selector,
    flags: Flags,
}

impl SGateArgs {
    pub fn new(rgate: &RecvGate) -> Self {
        SGateArgs {
            rgate_sel: rgate.sel(),
            label: 0,
            credits: 0,
            sel: INVALID_SEL,
            flags: Flags::empty(),
        }
    }

    pub fn credits(mut self, credits: u64) -> Self {
        self.credits = credits;
        self
    }

    pub fn label(mut self, label: dtu::Label) -> Self {
        self.label = label;
        self
    }

    pub fn sel(mut self, sel: Selector) -> Self {
        self.sel = sel;
        self
    }
}

impl SendGate {
    pub fn new(rgate: &RecvGate) -> Result<Self, Error> {
        Self::new_with(SGateArgs::new(rgate))
    }

    pub fn new_with(args: SGateArgs) -> Result<Self, Error> {
        let sel = if args.sel == INVALID_SEL {
            vpe::VPE::cur().alloc_cap()
        }
        else {
            args.sel
        };

        try!(syscalls::create_sgate(sel, args.rgate_sel, args.label, args.credits));
        Ok(SendGate {
            gate: Gate::new(sel, args.flags),
        })
    }

    pub fn new_bind(sel: Selector) -> Self {
        SendGate {
            gate: Gate::new(sel, Flags::KEEP_CAP),
        }
    }

    pub fn ep(&self) -> Option<dtu::EpId> {
        self.gate.ep
    }

    pub fn rebind(&mut self, sel: Selector) -> Result<(), Error> {
        self.gate.rebind(sel)
    }

    pub fn activate(&mut self) -> Result<EpId, Error> {
        self.gate.activate()
    }

    pub fn send<T>(&mut self, msg: &[T], reply_gate: &RecvGate) -> Result<(), Error> {
        let ep = try!(self.activate());
        dtu::DTU::send(ep, msg, 0, reply_gate.ep().unwrap())
    }
}