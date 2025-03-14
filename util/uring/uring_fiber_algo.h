// Copyright 2021, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/fiber_sched_algo.h"

namespace util {
namespace uring {
class Proactor;

using UringFiberProps  = FiberProps;

class UringFiberAlgo : public FiberSchedAlgo {

 public:
  explicit UringFiberAlgo(Proactor* proactor);
  ~UringFiberAlgo();

  int64_t suspend_time() const { return suspend_time_ns_; }

 private:
  void SuspendWithTimer(const time_point& tp) noexcept final;

  timespec ts_;

  int64_t suspend_time_ns_ = 0;
};

}  // namespace uring
}  // namespace util
