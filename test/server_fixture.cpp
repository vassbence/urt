#include <arpa/inet.h>
#include <array>
#include <netinet/in.h>
#include <server_fixture.h>
#include <sys/socket.h>
#include <tuple>
#include <unistd.h>
#include <urt.h>

server_fixture::server_fixture() {
  m_socket = socket(AF_INET, SOCK_STREAM, 0);
  int32_t opt = 1;
  setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(1234);

  std::ignore =
      bind(m_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
  listen(m_socket, 1);

  m_thread = std::thread([this]() {
    int32_t fd = accept(m_socket, nullptr, nullptr);
    if (fd < 0)
      return;

    std::string request_data;
    std::array<char, 1024> buffer;

    while (true) {
      ssize_t n = recv(fd, buffer.data(), buffer.size(), 0);
      if (n <= 0)
        break;

      request_data.append(buffer.data(), static_cast<size_t>(n));

      auto header_end = request_data.find("\r\n\r\n");
      if (header_end != std::string::npos) {
        size_t cl = 0;
        auto cl_pos = request_data.find("Content-Length: ");
        if (cl_pos != std::string::npos && cl_pos < header_end) {
          cl = std::stoull(request_data.substr(cl_pos + 16));
        }

        if (request_data.size() >= header_end + 4 + cl) {
          size_t space1 = request_data.find(' ');
          size_t space2 = request_data.find(' ', space1 + 1);
          std::string method = request_data.substr(0, space1);
          std::string path =
              request_data.substr(space1 + 1, space2 - space1 - 1);

          std::optional<std::string> response = std::nullopt;

          for (auto it = m_routes.begin(); it != m_routes.end(); ++it) {
            if (it->method == method && path.find(it->path) == 0) {
              response = it->handler(request_data);
              m_routes.erase(it); // Single-use consumption
              break;
            }
          }

          if (!response)
            break;

          send(fd, response->data(), response->size(), MSG_NOSIGNAL);
          request_data.erase(0, header_end + 4 + cl);
        }
      }
    }
    close(fd);
  });
}

server_fixture::~server_fixture() {
  if (m_socket != -1) {
    shutdown(m_socket, SHUT_RDWR);
    close(m_socket);
  }
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void server_fixture::register_handler(
    std::string method, std::string path,
    std::function<std::optional<std::string>(const std::string &)> handler) {
  m_routes.push_back({std::move(method), std::move(path), std::move(handler)});
}