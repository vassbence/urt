#include <gtest/gtest.h>
#include <server_fixture.h>
#include <string>
#include <urt.h>

TEST(build_success_response_path, appends_correct_request_id) {
  std::array<std::byte, urt::detail::aws::lambda::api::response_buffer_bytes>
      buffer;

  auto response_path =
      urt::detail::build_success_response_path(buffer, "a-b-c-d-1-2-3-4");

  EXPECT_EQ(response_path,
            "/2018-06-01/runtime/invocation/a-b-c-d-1-2-3-4/response");
}

TEST(build_error_response_path_and_body, appends_correct_request_id) {
  std::array<std::byte, urt::detail::aws::lambda::api::response_buffer_bytes>
      buffer;

  const auto &[path, body] = urt::detail::build_error_response_path_and_body(
      buffer, "a-b-c-d-1-2-3-4", "body");

  EXPECT_EQ(path, "/2018-06-01/runtime/invocation/a-b-c-d-1-2-3-4/error");
  EXPECT_EQ(body, "{\"errorMessage\":\"body\",\"errorType\":\"\","
                  "\"stackTrace\":[]}");
}

namespace {
template <typename T, typename = void>
struct can_be_passed_to_run : std::false_type {};

template <typename T>
struct can_be_passed_to_run<T,
                            std::void_t<decltype(urt::run(std::declval<T>()))>>
    : std::true_type {};
} // namespace

class run_test : public server_fixture {};

TEST_F(run_test, accepts_valid_fn_signatures) {
  auto valid_lambda = [](std::string_view) { return std::string{}; };
  EXPECT_TRUE(can_be_passed_to_run<decltype(valid_lambda)>::value);

  struct ValidFunctor {
    std::string operator()(std::string_view) const { return std::string{}; }
  };
  EXPECT_TRUE(can_be_passed_to_run<ValidFunctor>::value);
}

TEST_F(run_test, denies_invalid_fn_signautes) {
  auto mutable_lambda = [](std::string_view) mutable { return std::string{}; };
  EXPECT_FALSE(can_be_passed_to_run<decltype(mutable_lambda)>::value);

  auto wrong_return = [](std::string_view) { return 0; };
  EXPECT_FALSE(can_be_passed_to_run<decltype(wrong_return)>::value);

  auto wrong_param = [](int) { return std::string{}; };
  EXPECT_FALSE(can_be_passed_to_run<decltype(wrong_param)>::value);

  auto generic_lambda = [](const auto &) { return std::string{}; };
  EXPECT_FALSE(can_be_passed_to_run<decltype(generic_lambda)>::value);
}

TEST_F(run_test, expects_aws_lambda_runtime_api_address) {
  EXPECT_DEATH(
      { urt::run([](std::string_view) { return std::string{}; }); }, ".*");
}

TEST_F(run_test, expects_correct_length_aws_lambda_runtime_api_address) {
  std::string overly_long_address(129, 'a');
  setenv(urt::detail::aws::lambda::api::env_var.data(),
         overly_long_address.c_str(), 1);

  EXPECT_DEATH(
      { urt::run([](std::string_view) { return std::string{}; }); }, ".*");
}

TEST_F(run_test, ensures_request_id_header_is_present) {
  setenv(urt::detail::aws::lambda::api::env_var.data(), server_address, 1);

  register_handler("GET", "/2018-06-01/runtime/invocation/next",
                   [](const std::string &) {
                     return "HTTP/1.1 200 OK\r\n"
                            "Content-Length: 17\r\n\r\n"
                            "{\"event\":\"hello\"}";
                   });

  EXPECT_DEATH(
      { urt::run([](std::string_view) { return std::string{}; }); }, ".*");
}

TEST_F(run_test, ensures_event_payload_is_not_empty) {
  setenv(urt::detail::aws::lambda::api::env_var.data(), server_address, 1);

  register_handler("GET", "/2018-06-01/runtime/invocation/next",
                   [](const std::string &) {
                     return "HTTP/1.1 200 OK\r\n"
                            "Lambda-Runtime-Aws-Request-Id: req-123\r\n"
                            "Content-Length: 0\r\n\r\n";
                   });

  EXPECT_DEATH(
      { urt::run([](std::string_view) { return std::string{}; }); }, ".*");
}

TEST_F(run_test, handles_successful_handler_response) {
  setenv(urt::detail::aws::lambda::api::env_var.data(), server_address, 1);

  register_handler("GET", "/2018-06-01/runtime/invocation/next",
                   [](const std::string &) {
                     return "HTTP/1.1 200 OK\r\n"
                            "Lambda-Runtime-Aws-Request-Id: req-123\r\n"
                            "Content-Length: 17\r\n\r\n"
                            "{\"event\":\"hello\"}";
                   });

  register_handler(
      "POST", "/2018-06-01/runtime/invocation/", [](const std::string &req) {
        EXPECT_NE(req.find("{\"status\":\"success\"}"), std::string::npos);
        return "HTTP/1.1 202 Accepted\r\nContent-Length: 0\r\n\r\n";
      });

  EXPECT_DEATH(
      {
        urt::run([](std::string_view event) {
          EXPECT_EQ(event, "{\"event\":\"hello\"}");
          return std::string{"{\"status\":\"success\"}"};
        });
      },
      ".*");
}

TEST_F(run_test, reports_handler_thrown_error) {
  setenv(urt::detail::aws::lambda::api::env_var.data(), server_address, 1);

  register_handler("GET", "/2018-06-01/runtime/invocation/next",
                   [](const std::string &) {
                     return "HTTP/1.1 200 OK\r\n"
                            "Lambda-Runtime-Aws-Request-Id: req-123\r\n"
                            "Content-Length: 17\r\n\r\n"
                            "{\"event\":\"hello\"}";
                   });

  register_handler(
      "POST", "/2018-06-01/runtime/invocation/", [](const std::string &req) {
        EXPECT_NE(req.find("/req-123/error"), std::string::npos);
        EXPECT_NE(req.find("\"errorMessage\":\"foo\""), std::string::npos);

        return "HTTP/1.1 202 Accepted\r\nContent-Length: 0\r\n\r\n";
      });

  EXPECT_DEATH(
      {
        urt::run([](std::string_view) -> std::string {
          throw std::runtime_error("foo");
        });
      },
      ".*");
}