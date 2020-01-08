#include "ecn-mark-error-model.h"
#include "ns3/ppp-header.h"
#include "ns3/ipv4-header.h"

NS_OBJECT_ENSURE_REGISTERED(EcnMarkErrorModel);
 
TypeId EcnMarkErrorModel::GetTypeId(void) {
  static TypeId tid = TypeId("EcnMarkErrorModel")
    .SetParent<ErrorModel>()
    .AddConstructor<EcnMarkErrorModel>()
    ;
  return tid;
}
 
EcnMarkErrorModel::EcnMarkErrorModel() : enabled_(true) { }
 
bool EcnMarkErrorModel::DoCorrupt(Ptr<Packet> p) {
  if (enabled_) {
      PppHeader ppp;
      Ipv4Header hdr;
      p->RemoveHeader(ppp);
      p->RemoveHeader(hdr);
      if (hdr.GetEcn() != Ipv4Header::EcnType::ECN_NotECT) {
          hdr.SetEcn(Ipv4Header::EcnType::ECN_CE);
      }
      p->AddHeader(hdr);
      p->AddHeader(ppp);
  }
  return false;
}

void EcnMarkErrorModel::Enable() {
  enabled_ = true;
}

void EcnMarkErrorModel::Disable() {
  enabled_ = false;
}

void EcnMarkErrorModel::DoReset(void) { }

void Enable(Ptr<EcnMarkErrorModel> em, const Time next, const int repeat) {
    static int counter = 0;
    counter++;
    std::cout << Simulator::Now().GetSeconds() << "s: Enabling EcnMark" << std::endl;
    em->Enable();
    if(counter < repeat) {
        Simulator::Schedule(next, &Enable, em, next, repeat);
    }
}

void Disable(Ptr<EcnMarkErrorModel> em, const Time next, const int repeat) {
    static int counter = 0;
    std::cout << Simulator::Now().GetSeconds() << "s: Disabling EcnMark" << std::endl;
    counter++;
    em->Disable();
    if(counter < repeat) {
        Simulator::Schedule(next, &Disable, em, next, repeat);
    }
}