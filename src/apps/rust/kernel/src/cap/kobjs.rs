use base::cell::RefCell;
use base::col::String;
use base::dtu::{self, EpId, PEId, Label};
use base::GlobAddr;
use base::kif;
use base::rc::Rc;
use base::util;
use core::fmt;
use thread;

use com::SendQueue;
use mem;
use pes::{INVALID_VPE, VPE, VPEId};

#[derive(Clone)]
pub enum KObject {
    RGate(Rc<RefCell<RGateObject>>),
    SGate(Rc<RefCell<SGateObject>>),
    MGate(Rc<RefCell<MGateObject>>),
    Serv(Rc<RefCell<ServObject>>),
    Sess(Rc<RefCell<SessObject>>),
    VPE(Rc<RefCell<VPE>>),
}

impl fmt::Debug for KObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            KObject::SGate(ref s)   => write!(f, "{:?}", s.borrow()),
            KObject::RGate(ref r)   => write!(f, "{:?}", r.borrow()),
            KObject::MGate(ref m)   => write!(f, "{:?}", m.borrow()),
            KObject::Serv(ref s)    => write!(f, "{:?}", s.borrow()),
            KObject::Sess(ref s)    => write!(f, "{:?}", s.borrow()),
            KObject::VPE(ref v)     => write!(f, "{:?}", v.borrow()),
        }
    }
}

pub struct RGateObject {
    pub vpe: VPEId,
    pub ep: Option<EpId>,
    pub addr: usize,
    pub order: i32,
    pub msg_order: i32,
    pub header: usize,
}

impl RGateObject {
    pub fn new(order: i32, msg_order: i32) -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(RGateObject {
            vpe: INVALID_VPE,
            ep: None,
            addr: 0,
            order: order,
            msg_order: msg_order,
            header: 0,
        }))
    }

    pub fn activated(&self) -> bool {
        self.addr != 0
    }
    pub fn size(&self) -> usize {
        1 << self.order
    }
    pub fn msg_size(&self) -> usize {
        1 << self.msg_order
    }

    pub fn get_event(&self) -> thread::Event {
        self as *const Self as thread::Event
    }

    fn print_dest(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "VPE{}:", self.vpe)?;
        match self.ep {
            Some(ep) => write!(f, "EP{}", ep),
            None     => write!(f, "EP??"),
        }
    }
}

impl fmt::Debug for RGateObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "RGate[loc=")?;
        self.print_dest(f)?;
        write!(f, ", addr={:#x}, sz={:#x}, msz={:#x}, hd={:#x}]",
            self.addr, self.size(), self.msg_size(), self.header)
    }
}

pub struct SGateObject {
    pub rgate: Rc<RefCell<RGateObject>>,
    pub label: Label,
    pub credits: u64,
}

impl SGateObject {
    pub fn new(rgate: &Rc<RefCell<RGateObject>>, label: Label, credits: u64) -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(SGateObject {
            rgate: rgate.clone(),
            label: label,
            credits: credits,
        }))
    }
}

impl fmt::Debug for SGateObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "SGate[rgate=")?;
        self.rgate.borrow().print_dest(f)?;
        write!(f, ", lbl={:#x}, crd={}]", self.label, self.credits)
    }
}

pub struct MGateObject {
    pub pe: PEId,
    pub vpe: VPEId,
    pub addr: usize,
    pub size: usize,
    pub perms: kif::Perm,
    pub derived: bool,
}

impl MGateObject {
    pub fn new(pe: PEId, vpe: VPEId, addr: usize, size: usize, perms: kif::Perm) -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(MGateObject {
            pe: pe,
            vpe: vpe,
            addr: addr,
            size: size,
            perms: perms,
            derived: false,
        }))
    }
}

impl Drop for MGateObject {
    fn drop(&mut self) {
        // if it's not derived, it's always memory from mem-PEs
        if !self.derived {
            let gaddr = GlobAddr::new_with(self.pe, self.addr);
            mem::get().free(&mem::Allocation::new(gaddr, self.size));
        }
    }
}

impl fmt::Debug for MGateObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "MGate[tgt=VPE{}:PE{}, addr={:#x}, sz={:#x}, perm={:?}, der={}]",
            self.vpe, self.pe, self.addr, self.size, self.perms, self.derived)
    }
}

pub struct ServObject {
    pub name: String,
    pub rgate: Rc<RefCell<RGateObject>>,
    pub queue: SendQueue,
}

impl ServObject {
    pub fn new(vpe: &Rc<RefCell<VPE>>, name: String, rgate: Rc<RefCell<RGateObject>>) -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(ServObject {
            name: name,
            rgate: rgate.clone(),
            queue: SendQueue::new(vpe),
        }))
    }

    pub fn vpe(&self) -> &Rc<RefCell<VPE>> {
        self.queue.vpe()
    }

    pub fn send(&mut self, msg: &[u8]) -> Option<thread::Event> {
        let rep = self.rgate.borrow().ep.unwrap();
        self.queue.send(rep, msg, msg.len())
    }

    pub fn send_receive(serv: &Rc<RefCell<ServObject>>, msg: &[u8]) -> Option<&'static dtu::Message> {
        let event = serv.borrow_mut().send(msg);

        event.and_then(|event| {
            thread::ThreadManager::get().wait_for(event);
            thread::ThreadManager::get().fetch_msg()
        })
    }
}

impl fmt::Debug for ServObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Serv[name={}, rgate=", self.name)?;
        self.rgate.borrow().print_dest(f)?;
        write!(f, "]")
    }
}

pub struct SessObject {
    pub srv: Rc<RefCell<ServObject>>,
    pub ident: u64,
    pub srv_owned: bool,
}

impl SessObject {
    pub fn new(srv: &Rc<RefCell<ServObject>>, ident: u64, srv_owned: bool) -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(SessObject {
            srv: srv.clone(),
            ident: ident,
            srv_owned: srv_owned,
        }))
    }
}

impl Drop for SessObject {
    fn drop(&mut self) {
        let mut srv = self.srv.borrow_mut();
        if srv.vpe().borrow().has_app() {
            let smsg = kif::service::Close {
                opcode: kif::service::Operation::CLOSE.val as u64,
                sess: self.ident,
            };

            klog!(SERV, "Sending CLOSE(sess={:#x}) to service {}", self.ident, srv.name);
            srv.send(util::object_to_bytes(&smsg));
        }
    }
}

impl fmt::Debug for SessObject {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Sess[service={}, ident={:#x}]", self.srv.borrow().name, self.ident)
    }
}
