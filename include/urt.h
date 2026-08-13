#ifndef URT_H
#define URT_H

#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <netinet/in.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <type_traits>
#include <unistd.h>
#include <utility>

namespace urt::detail {
inline constexpr size_t receive_chunk_bytes = 64 * 1024;
inline constexpr std::string_view colon = ":";
inline constexpr std::string_view crlf = "\r\n";
inline constexpr std::string_view space = " ";
inline constexpr std::string_view null = "\0";

namespace http {
inline constexpr size_t max_supported_header_count = 64;
inline constexpr size_t max_supported_host_bytes = 128;
inline constexpr std::string_view version_1_1 = "HTTP/1.1";
inline constexpr std::string_view transfer_encoding_chunked = "chunked";

namespace method {
inline constexpr std::string_view get = "GET";
inline constexpr std::string_view post = "POST";
} // namespace method

namespace header {
inline constexpr std::string_view host = "Host";
inline constexpr std::string_view content_type = "Content-Type";
inline constexpr std::string_view content_length = "Content-Length";
inline constexpr std::string_view transfer_encoding = "Transfer-Encoding";
} // namespace header

namespace content_type {
inline constexpr std::string_view text = "text/plain";
inline constexpr std::string_view json = "application/json";
} // namespace content_type
} // namespace http

namespace aws::lambda {
inline constexpr std::string_view request_id_header =
    "Lambda-Runtime-Aws-Request-Id";

namespace payload {
inline constexpr size_t max_head_bytes = 1024 * 1024;
inline constexpr size_t max_body_bytes = 6 * 1024 * 1024;
inline constexpr size_t max_bytes = max_head_bytes + max_body_bytes;
} // namespace payload

namespace api {
inline constexpr std::string_view env_var = "AWS_LAMBDA_RUNTIME_API";
inline constexpr std::string_view next_invocation_path =
    "/2018-06-01/runtime/invocation/next";
inline constexpr std::string_view invocation_prefix =
    "/2018-06-01/runtime/invocation/";
inline constexpr std::string_view next_suffix = "/response";
inline constexpr std::string_view error_suffix = "/error";
inline constexpr size_t max_error_body_bytes = 4 * 1024;
inline constexpr size_t response_buffer_bytes =
    invocation_prefix.size() + http::max_supported_host_bytes +
    std::max(next_suffix.size(), error_suffix.size()) + max_error_body_bytes;
} // namespace api
} // namespace aws::lambda

struct address {
  // address as uri authority parts
  std::string_view host;
  uint16_t port;

  // address converted to network address
  sockaddr_in network_address;

  explicit address(std::string_view authority);
};

struct connection {
  int32_t socket;

  explicit connection(const address &address);

  ~connection();
};

namespace http {
// has a separate field for head and body to avoid double
// buffering the body
struct request {
  std::string_view head;
  std::string_view body;

  explicit request(
      std::array<std::byte, aws::lambda::payload::max_bytes> &buffer,
      std::string_view method, std::string_view path, std::string_view host,
      std::string_view body = {}, std::string_view content_type = {});
};

void send_request(int32_t socket, const request &request);

struct response {
  uint16_t status;
  uint8_t headers_size;
  std::array<std::pair<std::string_view, std::string_view>,
             http::max_supported_header_count>
      headers;
  std::string_view body;
};

response fetch(std::array<std::byte, aws::lambda::payload::max_bytes> &buffer,
               int32_t socket, std::string_view method, std::string_view path,
               std::string_view host, std::string_view body = {},
               std::string_view content_type = {});

enum class framing { chunked, fixed };

enum class parser_state {
  status,
  headers,
  chunked_size,
  chunked_data,
  fixed_body,
  done
};

response
receive_response(std::array<std::byte, aws::lambda::payload::max_bytes> &buffer,
                 int32_t socket);
}; // namespace http
}; // namespace urt::detail

namespace urt::detail {
std::string_view build_success_response_path(
    std::array<std::byte, aws::lambda::api::response_buffer_bytes> &buffer,
    std::string_view request_id);

std::pair<std::string_view, std::string_view>
build_error_response_path_and_body(
    std::array<std::byte, aws::lambda::api::response_buffer_bytes> &buffer,
    std::string_view request_id, std::string_view error);
} // namespace urt::detail

namespace urt {
struct event {
  std::string_view payload;
};

struct response {
  std::string payload;
  std::string_view content_type;
};

template <
    typename Fn,
    typename = std::enable_if_t<std::is_same_v<
        decltype(&Fn::operator()), response (Fn::*)(const event &) const>>>
void run(Fn fn);
}; // namespace urt

// as per https://github.com/microsoft/GSL/blob/main/include/gsl/assert,
// https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#i6-prefer-expects-for-expressing-preconditions
#define EXPECTS(x)                                                             \
  (__builtin_expect(!!(x), 1) ? static_cast<void>(0) : std::terminate())
#define ENSURES(x)                                                             \
  (__builtin_expect(!!(x), 1) ? static_cast<void>(0) : std::terminate())

inline urt::detail::connection::connection(const address &address) {
  socket = ::socket(AF_INET, SOCK_STREAM, 0);
  ENSURES(socket != -1);

  int32_t connect_result = connect(
      socket, reinterpret_cast<const sockaddr *>(&address.network_address),
      sizeof(address.network_address));
  ENSURES(connect_result != -1);
};

inline urt::detail::connection::~connection() {
  if (socket != -1) {
    close(socket);
  }
}

inline urt::detail::address::address(std::string_view authority) {
  EXPECTS(authority.size() > 0);

  size_t colon_position = authority.find(colon);
  port = 80;

  if (colon_position != std::string_view::npos) {
    std::string_view port_view = authority.substr(colon_position + 1);
    std::from_chars(port_view.data(), port_view.data() + port_view.size(),
                    port);
  }

  host = authority.substr(0, colon_position);
  // smaller than operator because INET_ADDRSTRLEN is 16 (IPv4 has
  // a max size of 15 + 1 null terminator character)
  EXPECTS(host.size() < INET_ADDRSTRLEN);
  std::array<char, INET_ADDRSTRLEN> null_terminated_host;
  std::memcpy(null_terminated_host.data(), host.data(), host.size());
  null_terminated_host[host.size()] = '\0';

  network_address = {};
  network_address.sin_family = AF_INET;
  network_address.sin_port = htons(port);

  int32_t inet_conversion_result = inet_pton(
      AF_INET, null_terminated_host.data(), &network_address.sin_addr);
  ENSURES(inet_conversion_result == 1);
};

inline urt::detail::http::request::request(
    std::array<std::byte, aws::lambda::payload::max_bytes> &buffer,
    std::string_view method, std::string_view path, std::string_view host,
    std::string_view body, std::string_view content_type) {
  EXPECTS(!method.empty());
  EXPECTS(!path.empty());
  EXPECTS(!host.empty());
  EXPECTS((method == http::method::get && body.empty()) ||
          (method != http::method::get && !body.empty()));
  EXPECTS((method == http::method::get && content_type.empty()) ||
          (method != http::method::get && !content_type.empty()));

  size_t n = 0;

  // {METHOD} {PATH} HTTP/1.1\r\n
  std::memcpy(buffer.data(), method.data(), method.size());
  n += method.size();
  std::memcpy(buffer.data() + n, space.data(), space.size());
  n += space.size();
  std::memcpy(buffer.data() + n, path.data(), path.size());
  n += path.size();
  std::memcpy(buffer.data() + n, space.data(), space.size());
  n += space.size();
  std::memcpy(buffer.data() + n, http::version_1_1.data(),
              http::version_1_1.size());
  n += http::version_1_1.size();
  std::memcpy(buffer.data() + n, crlf.data(), crlf.size());
  n += crlf.size();

  // Host: {HOST}\r\n
  std::memcpy(buffer.data() + n, http::header::host.data(),
              http::header::host.size());
  n += http::header::host.size();
  std::memcpy(buffer.data() + n, colon.data(), colon.size());
  n += colon.size();
  std::memcpy(buffer.data() + n, space.data(), space.size());
  n += space.size();
  std::memcpy(buffer.data() + n, host.data(), host.size());
  n += host.size();
  std::memcpy(buffer.data() + n, crlf.data(), crlf.size());
  n += crlf.size();

  if (method != http::method::get) {
    // Content-Length: {CALCULATED_CONTENT_LENGTH}\r\n
    std::memcpy(buffer.data() + n, http::header::content_length.data(),
                http::header::content_length.size());
    n += http::header::content_length.size();
    std::memcpy(buffer.data() + n, colon.data(), colon.size());
    n += colon.size();
    std::memcpy(buffer.data() + n, space.data(), space.size());
    n += space.size();
    auto [ptr, ec] = std::to_chars(
        reinterpret_cast<char *>(buffer.data() + n),
        reinterpret_cast<char *>(buffer.data() + buffer.size()), body.size());
    ENSURES(ec == std::errc{});
    n = static_cast<size_t>(ptr - reinterpret_cast<char *>(buffer.data()));
    std::memcpy(buffer.data() + n, crlf.data(), crlf.size());
    n += crlf.size();

    // Content-Type: {CONTENT_TYPE}\r\n
    std::memcpy(buffer.data() + n, http::header::content_type.data(),
                http::header::content_type.size());
    n += http::header::content_type.size();
    std::memcpy(buffer.data() + n, colon.data(), colon.size());
    n += colon.size();
    std::memcpy(buffer.data() + n, space.data(), space.size());
    n += space.size();
    std::memcpy(buffer.data() + n, content_type.data(), content_type.size());
    n += content_type.size();
    std::memcpy(buffer.data() + n, crlf.data(), crlf.size());
    n += crlf.size();
  }

  // \r\n
  std::memcpy(buffer.data() + n, crlf.data(), crlf.size());
  n += crlf.size();

  head = std::string_view{reinterpret_cast<const char *>(buffer.data()), n};
  this->body = body;
};

inline urt::detail::http::response urt::detail::http::receive_response(
    std::array<std::byte, aws::lambda::payload::max_bytes> &buffer,
    int32_t socket) {
  EXPECTS(socket > -1);

  parser_state state = parser_state::status;
  size_t head = 0;
  size_t tail = 0;

  framing framing = framing::fixed;
  // equals Content-Length if HTTP_FRAMING_KIND_FIXED or the size
  // of the current chunk if HTTP_FRAMING_KIND_CHUNKED
  size_t framing_size = 0;
  uint16_t status = 0;
  uint8_t headers_size = 0;
  std::array<std::pair<std::string_view, std::string_view>,
             http::max_supported_header_count>
      headers{};
  std::string_view body;

  bool need_more_data = true;

  while (state != parser_state::done) {
    if (need_more_data) {
      ssize_t n = recv(socket, buffer.data() + tail, receive_chunk_bytes, 0);
      if (n <= 0) {
        break;
      }

      tail += static_cast<size_t>(n);
      need_more_data = false;
    }

    std::string_view view(reinterpret_cast<const char *>(buffer.data() + head),
                          tail - head);

    switch (state) {
    case parser_state::status: {
      size_t end = view.find(crlf);
      if (end == std::string_view::npos) {
        need_more_data = true;
        break;
      }

      std::string_view line = view.substr(0, end);
      size_t first_space = line.find(space);
      size_t second_space = line.find(space, first_space + 1);

      std::string_view raw_status =
          line.substr(first_space + 1, second_space - first_space - 1);
      std::from_chars(raw_status.data(), raw_status.data() + raw_status.size(),
                      status);

      head += end + 2;
      state = parser_state::headers;
      break;
    }
    case parser_state::headers: {
      size_t end = view.find(crlf);
      if (end == std::string_view::npos) {
        need_more_data = true;
        break;
      }

      while (end != std::string_view::npos) {
        if (end == 0) {
          head += 2;

          state = (framing == framing::chunked) ? parser_state::chunked_size
                                                : parser_state::fixed_body;

          break;
        }

        std::string_view line = view.substr(0, end);
        size_t colon_position = line.find(colon);
        std::string_view key = line.substr(0, colon_position);
        std::string_view value = line.substr(colon_position + 2);

        headers[headers_size++] = {key, value};

        if (key == http::header::transfer_encoding &&
            value.find(http::transfer_encoding_chunked) !=
                std::string_view::npos) {
          framing = framing::chunked;
        } else if (key == http::header::content_length) {
          std::from_chars(value.data(), value.data() + value.size(),
                          framing_size);
        }

        head += end + 2;
        view = std::string_view(
            reinterpret_cast<const char *>(buffer.data() + head), tail - head);
        end = view.find(crlf);
      }
      break;
    }
    case parser_state::chunked_size: {
      size_t end = view.find(crlf);
      if (end == std::string_view::npos) {
        need_more_data = true;
        break;
      }

      std::string_view line = view.substr(0, end);
      std::from_chars(line.data(), line.data() + line.size(), framing_size, 16);

      head += end + 2;

      if (framing_size == 0) {
        head += 2;
        state = parser_state::done;
      } else {
        if (body.data() == nullptr) {
          body = std::string_view(
              reinterpret_cast<const char *>(buffer.data() + head), 0);
        }
        state = parser_state::chunked_data;
      }
      break;
    }
    case parser_state::chunked_data: {
      if (tail - head < framing_size + 2) {
        need_more_data = true;
        break;
      }

      std::memmove(buffer.data() +
                       (reinterpret_cast<const std::byte *>(body.data()) -
                        buffer.data()) +
                       body.size(),
                   buffer.data() + head, framing_size);

      body = std::string_view(body.data(), body.size() + framing_size);
      head += framing_size + 2;
      state = parser_state::chunked_size;
      break;
    }
    case parser_state::fixed_body: {
      if (tail - head < framing_size) {
        need_more_data = true;
        break;
      }

      body = std::string_view(
          reinterpret_cast<const char *>(buffer.data() + head), framing_size);
      head += framing_size;
      state = parser_state::done;
      break;
    }

    case parser_state::done:
      break;
    }
  }

  return response{status, headers_size, std::move(headers), body};
}

inline void urt::detail::http::send_request(int32_t socket,
                                            const request &request) {
  EXPECTS(socket > -1);

  for (size_t i = 0; i < request.head.size();) {
    ssize_t n = send(socket, request.head.data() + i, request.head.size() - i,
                     MSG_NOSIGNAL);
    ENSURES(n != -1);
    i += static_cast<size_t>(n);
  }

  for (size_t i = 0; i < request.body.size();) {
    ssize_t n = send(socket, request.body.data() + i, request.body.size() - i,
                     MSG_NOSIGNAL);
    ENSURES(n != -1);
    i += static_cast<size_t>(n);
  }
};

inline std::string_view urt::detail::build_success_response_path(
    std::array<std::byte, aws::lambda::api::response_buffer_bytes> &buffer,
    std::string_view request_id) {
  size_t n = 0;

  std::memcpy(buffer.data(), aws::lambda::api::invocation_prefix.data(),
              aws::lambda::api::invocation_prefix.size());
  n += aws::lambda::api::invocation_prefix.size();

  std::memcpy(buffer.data() + n, request_id.data(), request_id.size());
  n += request_id.size();

  std::memcpy(buffer.data() + n, aws::lambda::api::next_suffix.data(),
              aws::lambda::api::next_suffix.size());
  n += aws::lambda::api::next_suffix.size();

  std::memcpy(buffer.data() + n, null.data(), null.size());
  n += null.size();

  return {std::string_view{reinterpret_cast<const char *>(buffer.data()), n}};
}

inline std::pair<std::string_view, std::string_view>
urt::detail::build_error_response_path_and_body(
    std::array<std::byte, aws::lambda::api::response_buffer_bytes> &buffer,
    std::string_view request_id, std::string_view error) {
  size_t n = 0;

  std::memcpy(buffer.data(), aws::lambda::api::invocation_prefix.data(),
              aws::lambda::api::invocation_prefix.size());
  n += aws::lambda::api::invocation_prefix.size();

  std::memcpy(buffer.data() + n, request_id.data(), request_id.size());
  n += request_id.size();

  std::memcpy(buffer.data() + n, aws::lambda::api::error_suffix.data(),
              aws::lambda::api::error_suffix.size());
  n += aws::lambda::api::error_suffix.size();

  std::memcpy(buffer.data() + n, null.data(), null.size());
  n += null.size();

  std::string_view path{reinterpret_cast<const char *>(buffer.data()), n};

  size_t body_start = n;
  constexpr std::string_view json_start = "{\"errorMessage\":\"";
  constexpr std::string_view json_end =
      "\",\"errorType\":\"\",\"stackTrace\":[]}";
  constexpr size_t fixed_json_size = json_start.size() + json_end.size();

  size_t available_space =
      aws::lambda::api::max_error_body_bytes - fixed_json_size;
  size_t msg_len = std::min(error.size(), available_space);

  std::memcpy(buffer.data() + n, json_start.data(), json_start.size());
  n += json_start.size();

  std::memcpy(buffer.data() + n, error.data(), msg_len);
  n += msg_len;

  std::memcpy(buffer.data() + n, json_end.data(), json_end.size());
  n += json_end.size();

  std::string_view body{
      reinterpret_cast<const char *>(buffer.data() + body_start),
      n - body_start};

  return {path, body};
}

inline urt::detail::http::response urt::detail::http::fetch(
    std::array<std::byte, aws::lambda::payload::max_bytes> &buffer,
    int32_t socket, std::string_view method, std::string_view path,
    std::string_view host, std::string_view body,
    std::string_view content_type) {
  request request(buffer, method, path, host, body, content_type);

  send_request(socket, request);

  return receive_response(buffer, socket);
};

template <typename Fn, typename> void urt::run(Fn fn) {
  const char *aws_lambda_runtime_api_address =
      std::getenv(detail::aws::lambda::api::env_var.data());
  EXPECTS(aws_lambda_runtime_api_address != nullptr);
  EXPECTS(strlen(aws_lambda_runtime_api_address) <=
          detail::http::max_supported_host_bytes);

  std::array<std::byte, detail::aws::lambda::payload::max_bytes> buffer;
  std::array<std::byte, detail::aws::lambda::api::response_buffer_bytes>
      response_buffer;

  detail::address parsed_aws_lambda_runtime_api_address(
      (std::string_view(aws_lambda_runtime_api_address)));

  detail::connection aws_lambda_runtime_api_connection(
      parsed_aws_lambda_runtime_api_address);

  for (;;) {
    detail::http::response get_next_invocation_response =
        detail::http::fetch(buffer, aws_lambda_runtime_api_connection.socket,
                            detail::http::method::get,
                            detail::aws::lambda::api::next_invocation_path,
                            aws_lambda_runtime_api_address);
    ENSURES(get_next_invocation_response.status == 200);

    std::string_view request_id;
    for (size_t i = 0; i < get_next_invocation_response.headers_size; ++i) {
      const auto &[key, value] = get_next_invocation_response.headers[i];
      if (key == detail::aws::lambda::request_id_header) {
        request_id = value;
        break;
      }
    }
    ENSURES(!request_id.empty());

    const event event{get_next_invocation_response.body};
    ENSURES(!event.payload.empty());

    try {
      const response response = fn(event);

      // as per
      // https://docs.aws.amazon.com/lambda/latest/dg/runtimes-api.html#runtimes-api-next
      detail::http::response post_invocation_outcome_response =
          detail::http::fetch(
              buffer, aws_lambda_runtime_api_connection.socket,
              detail::http::method::post,
              detail::build_success_response_path(response_buffer, request_id),
              aws_lambda_runtime_api_address, response.payload,
              response.content_type);
      ENSURES(post_invocation_outcome_response.status == 202);
    } catch (const std::exception &exception) {
      // as per
      // https://docs.aws.amazon.com/lambda/latest/dg/runtimes-api.html#runtimes-api-invokeerror
      const auto &[path, body] = detail::build_error_response_path_and_body(
          response_buffer, request_id, exception.what());

      detail::http::response post_invocation_error_response =
          detail::http::fetch(buffer, aws_lambda_runtime_api_connection.socket,
                              detail::http::method::post, path,
                              aws_lambda_runtime_api_address, body,
                              detail::http::content_type::json);
      ENSURES(post_invocation_error_response.status == 202);
    }
  };
}

#endif