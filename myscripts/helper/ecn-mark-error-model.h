#ifndef ECN_MARK_ERROR_MODEL_H
#define ECN_MARK_ERROR_MODEL_H

#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/socket.h"

using namespace ns3;

// The EcnMark marks all packets.
class EcnMarkErrorModel : public ErrorModel {
public:
  static TypeId GetTypeId(void);
  EcnMarkErrorModel();

  void Enable();
  void Disable();
 
private:
  bool enabled_;
  bool DoCorrupt (Ptr<Packet> p);
  void DoReset(void);
};

void Disable(Ptr<EcnMarkErrorModel> em, const Time next, const int repeat);
void Enable(Ptr<EcnMarkErrorModel> em, const Time next, const int repeat);

#endif /* ECN_MARK_ERROR_MODEL_H */
