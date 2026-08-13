#include <gtest/gtest.h>
#include <urt.h>

class socket_fixture : public testing::Test {
protected:
  int32_t server_socket;
  int32_t client_socket;

  socket_fixture() {
    int32_t sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    server_socket = sv[0];
    client_socket = sv[1];
  }

  ~socket_fixture() {
    close(server_socket);
    close(client_socket);
  }
};

TEST(http_request_test, get_request) {
  std::array<std::byte, urt::detail::aws::lambda::payload::max_bytes> buffer;
  urt::detail::http::request request(buffer, "GET", "/", "127.0.0.1");

  EXPECT_EQ(std::string{request.head} + std::string{request.body},
            "GET / HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "\r\n");
};

TEST(http_request_test, post_request) {
  std::array<std::byte, urt::detail::aws::lambda::payload::max_bytes> buffer;
  urt::detail::http::request request(buffer, "POST", "/", "127.0.0.1",
                                     "{\"key\":\"value\"}", "application/json");

  EXPECT_EQ(std::string{request.head} + std::string{request.body},
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Content-Length: 15\r\n"
            "Content-Type: application/json\r\n"
            "\r\n"
            "{\"key\":\"value\"}");
};

class http_receive_response_test : public socket_fixture {};

TEST_F(http_receive_response_test, fixed_response) {
  std::string response = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: 15\r\n"
                         "\r\n"
                         "{\"status\":\"ok\"}";
  send(server_socket, response.c_str(), response.length(), 0);
  std::array<std::byte, urt::detail::aws::lambda::payload::max_bytes> buffer;

  auto parsed_response =
      urt::detail::http::receive_response(buffer, client_socket);

  EXPECT_EQ(parsed_response.status, 200);
  EXPECT_EQ(parsed_response.headers_size, 2);
  EXPECT_EQ(parsed_response.headers[0].first, "Content-Type");
  EXPECT_EQ(parsed_response.headers[0].second, "application/json");
  EXPECT_EQ(parsed_response.headers[1].first, "Content-Length");
  EXPECT_EQ(parsed_response.headers[1].second, "15");
  EXPECT_EQ(parsed_response.body, "{\"status\":\"ok\"}");
}

TEST_F(http_receive_response_test, fragmented_fixed_response) {
  std::array<std::byte, urt::detail::aws::lambda::payload::max_bytes> buffer;
  std::string part1 = "HTTP/1.1 200 OK\r\n";
  send(server_socket, part1.c_str(), part1.length(), 0);
  std::string part2 = "Content-Length: 5\r\n\r\nHel";
  send(server_socket, part2.c_str(), part2.length(), 0);
  std::string part3 = "lo!";
  send(server_socket, part3.c_str(), part3.length(), 0);

  auto parsed_response =
      urt::detail::http::receive_response(buffer, client_socket);

  EXPECT_EQ(parsed_response.status, 200);
  EXPECT_EQ(parsed_response.body, "Hello");
}

TEST_F(http_receive_response_test, chunked_response) {
  std::array<std::byte, urt::detail::aws::lambda::payload::max_bytes> buffer;
  std::string response = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/plain\r\n"
                         "Transfer-Encoding: chunked\r\n"
                         "\r\n"
                         "7\r\n"
                         "Hello, \r\n"
                         "6\r\n"
                         "world!\r\n"
                         "0\r\n"
                         "\r\n";
  send(server_socket, response.c_str(), response.length(), 0);

  auto parsed_response =
      urt::detail::http::receive_response(buffer, client_socket);

  EXPECT_EQ(parsed_response.status, 200);
  EXPECT_EQ(parsed_response.headers_size, 2);
  EXPECT_EQ(parsed_response.headers[0].first, "Content-Type");
  EXPECT_EQ(parsed_response.headers[0].second, "text/plain");
  EXPECT_EQ(parsed_response.headers[1].first, "Transfer-Encoding");
  EXPECT_EQ(parsed_response.headers[1].second, "chunked");
  EXPECT_EQ(parsed_response.body, "Hello, world!");
}

TEST_F(http_receive_response_test, fragmented_chunked_response) {
  std::array<std::byte, urt::detail::aws::lambda::payload::max_bytes> buffer;
  std::string part1 = "HTTP/1.1 200 OK\r\n"
                      "Transfer-Encoding: chunked\r\n"
                      "\r\n"
                      "5\r\n"
                      "Hello";
  send(server_socket, part1.c_str(), part1.length(), 0);
  std::string part2 = "\r\n"
                      "6\r\n"
                      " world";
  send(server_socket, part2.c_str(), part2.length(), 0);
  std::string part3 = "\r\n"
                      "0\r\n"
                      "\r\n";
  send(server_socket, part3.c_str(), part3.length(), 0);

  auto parsed_response =
      urt::detail::http::receive_response(buffer, client_socket);

  EXPECT_EQ(parsed_response.status, 200);
  EXPECT_EQ(parsed_response.headers_size, 1);
  EXPECT_EQ(parsed_response.headers[0].first, "Transfer-Encoding");
  EXPECT_EQ(parsed_response.headers[0].second, "chunked");
  EXPECT_EQ(parsed_response.body, "Hello world");
}

TEST_F(http_receive_response_test, error_status_code) {
  std::array<std::byte, urt::detail::aws::lambda::payload::max_bytes> buffer;
  std::string response = "HTTP/1.1 502 Bad Gateway\r\n"
                         "Content-Length: 0\r\n"
                         "\r\n";
  send(server_socket, response.c_str(), response.length(), 0);

  auto parsed_response =
      urt::detail::http::receive_response(buffer, client_socket);

  EXPECT_EQ(parsed_response.status, 502);
  EXPECT_TRUE(parsed_response.body.empty());
}

class http_send_request_test : public socket_fixture {};

TEST_F(http_send_request_test, send_get_request) {
  std::array<std::byte, urt::detail::aws::lambda::payload::max_bytes> buffer;
  urt::detail::http::request request(buffer, "GET", "/", "127.0.0.1");

  send_request(client_socket, request);

  std::string received(request.head.size(), '\0');
  ASSERT_EQ(recv(server_socket, received.data(), received.size(), 0),
            static_cast<ssize_t>(received.size()));
  EXPECT_EQ(received, "GET / HTTP/1.1\r\n"
                      "Host: 127.0.0.1\r\n"
                      "\r\n");
}

TEST_F(http_send_request_test, send_post_request_with_body) {
  std::array<std::byte, urt::detail::aws::lambda::payload::max_bytes> buffer;
  urt::detail::http::request request(buffer, "POST", "/api", "127.0.0.1",
                                     "{\"foo\":\"bar\"}", "application/json");

  send_request(client_socket, request);

  std::string received(request.head.size() + request.body.size(), '\0');
  ASSERT_EQ(recv(server_socket, received.data(), received.size(), 0),
            static_cast<ssize_t>(received.size()));
  EXPECT_EQ(received, "POST /api HTTP/1.1\r\n"
                      "Host: 127.0.0.1\r\n"
                      "Content-Length: 13\r\n"
                      "Content-Type: application/json\r\n"
                      "\r\n"
                      "{\"foo\":\"bar\"}");
}

class http_fetch_test : public socket_fixture {};

TEST_F(http_fetch_test, builds_sends_and_reveives_request_correctly) {
  std::string server_sent_response = "HTTP/1.1 200 OK\r\n"
                                     "Content-Type: application/json\r\n"
                                     "Content-Length: 15\r\n"
                                     "\r\n"
                                     "{\"status\":\"ok\"}";
  send(server_socket, server_sent_response.c_str(),
       server_sent_response.length(), 0);
  std::array<std::byte, urt::detail::aws::lambda::payload::max_bytes> buffer;

  auto response =
      urt::detail::http::fetch(buffer, client_socket, "GET", "/", "127.0.0.1");

  EXPECT_EQ(response.status, 200);
  EXPECT_EQ(response.headers_size, 2);
  EXPECT_EQ(response.headers[0].first, "Content-Type");
  EXPECT_EQ(response.headers[0].second, "application/json");
  EXPECT_EQ(response.headers[1].first, "Content-Length");
  EXPECT_EQ(response.headers[1].second, "15");
  EXPECT_EQ(response.body, "{\"status\":\"ok\"}");
  std::string expected_request_sent_to_server = "GET / HTTP/1.1\r\n"
                                                "Host: 127.0.0.1\r\n"
                                                "\r\n";
  std::string received_request(expected_request_sent_to_server.size(), '\0');
  EXPECT_EQ(
      recv(server_socket, received_request.data(), received_request.size(), 0),
      static_cast<ssize_t>(expected_request_sent_to_server.size()));
  EXPECT_EQ(received_request, expected_request_sent_to_server);
}