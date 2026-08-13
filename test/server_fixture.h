#ifndef SERVER_FIXTURE_H
#define SERVER_FIXTURE_H

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <urt.h>

class server_fixture : public testing::Test {
private:
  int32_t m_socket = -1;
  std::thread m_thread;

  struct route {
    std::string method;
    std::string path;
    std::function<std::optional<std::string>(const std::string &)> handler;
  };
  std::vector<route> m_routes;

protected:
  const char *server_address = "127.0.0.1:1234";

  server_fixture();
  ~server_fixture() override;

  void register_handler(
      std::string method, std::string path,
      std::function<std::optional<std::string>(const std::string &)> handler);
};

#endif