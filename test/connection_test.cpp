#include <gtest/gtest.h>
#include <server_fixture.h>
#include <string>
#include <urt.h>

class connection_test : public server_fixture {};

TEST_F(connection_test, connects_to_working_address) {
  urt::detail::connection connection((urt::detail::address(server_address)));

  EXPECT_GT(connection.socket, -1);
};

TEST_F(connection_test, fails_if_address_doesnt_work) {
  EXPECT_DEATH(urt::detail::connection connection(
                   urt::detail::address("127.0.0.1:12345")),
               ".*");
};
