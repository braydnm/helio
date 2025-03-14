// Copyright 2021, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/accept_server.h"

#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>

#include "base/gtest.h"
#include "base/logging.h"
#include "util/asio_stream_adapter.h"
#include "util/listener_interface.h"
#include "util/uring/uring_pool.h"
#include "util/epoll/ev_pool.h"

namespace util {
namespace uring {

using namespace std;
namespace h2 = boost::beast::http;
using fibers_ext::BlockingCounter;

#define USE_URING 1

class TestConnection : public Connection {
 protected:
  void HandleRequests() final;
};

void TestConnection::HandleRequests() {
  char buf[128];
  boost::system::error_code ec;

  AsioStreamAdapter<FiberSocketBase> asa(*socket_);

  while (true) {
    asa.read_some(boost::asio::buffer(buf), ec);
    if (ec == std::errc::connection_aborted)
      break;

    CHECK(!ec) << ec << "/" << ec.message();

    asa.write_some(boost::asio::buffer(buf), ec);

    if (FiberSocketBase::IsConnClosed(ec))
      break;

    CHECK(!ec);
  }
  VLOG(1) << "TestConnection exit";
}

class TestListener : public ListenerInterface {
 public:
  virtual Connection* NewConnection(ProactorBase* context) final {
    return new TestConnection;
  }
};

class AcceptServerTest : public testing::Test {
 protected:
  void SetUp() override;

  void TearDown() override {
    as_->Stop(true);
    pp_->Stop();
  }

  static void SetUpTestCase() {
  }

  std::unique_ptr<ProactorPool> pp_;
  std::unique_ptr<AcceptServer> as_;
  std::unique_ptr<FiberSocketBase> client_sock_;
};

void AcceptServerTest::SetUp() {
  const uint16_t kPort = 1234;
#ifdef USE_URING
  ProactorPool* up = new UringPool(16, 2);
#else
  ProactorPool* up = new epoll::EvPool(2);
#endif
  pp_.reset(up);
  pp_->Run();

  as_.reset(new AcceptServer{up});
  as_->AddListener("localhost", kPort, new TestListener);
  as_->Run();

  ProactorBase* pb = pp_->GetNextProactor();
  client_sock_.reset(pb->CreateSocket());
  auto address = boost::asio::ip::make_address("127.0.0.1");
  FiberSocketBase::endpoint_type ep{address, kPort};

  pb->Await([&] {
    FiberSocketBase::error_code ec = client_sock_->Connect(ep);
    CHECK(!ec) << ec;
  });
}

void RunClient(FiberSocketBase* fs) {
  LOG(INFO) << ": Ping-client started";
  AsioStreamAdapter<> asa(*fs);

  ASSERT_TRUE(fs->IsOpen());

  h2::request<h2::string_body> req(h2::verb::get, "/", 11);
  req.body().assign("foo");
  req.prepare_payload();
  h2::write(asa, req);

  LOG(INFO) << ": echo-client stopped";
}

TEST_F(AcceptServerTest, Basic) {
  client_sock_->proactor()->Await([&] { RunClient(client_sock_.get()); });
}

TEST_F(AcceptServerTest, Break) {
  usleep(1000);
  as_->Stop(true);
}

}  // namespace uring
}  // namespace util
