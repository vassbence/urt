#include <gtest/gtest.h>
#include <urt.h>

TEST(address_test, without_port) {
  urt::detail::address address("127.0.0.1");

  EXPECT_EQ(address.host, "127.0.0.1");
  EXPECT_EQ(address.port, 80);
  EXPECT_EQ(address.network_address.sin_family, AF_INET);
  EXPECT_EQ(address.network_address.sin_port, htons(80));
  EXPECT_EQ(address.network_address.sin_addr.s_addr, htonl(INADDR_LOOPBACK));
};

TEST(address_test, with_port) {
  urt::detail::address address("127.0.0.1:1234");

  EXPECT_EQ(address.host, "127.0.0.1");
  EXPECT_EQ(address.port, 1234);
  EXPECT_EQ(address.network_address.sin_family, AF_INET);
  EXPECT_EQ(address.network_address.sin_port, htons(1234));
  EXPECT_EQ(address.network_address.sin_addr.s_addr, htonl(INADDR_LOOPBACK));
};
