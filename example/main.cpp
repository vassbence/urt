#include <urt.h>

int main() {
  urt::run([](const urt::event &event) {
    return urt::response{std::string(event.payload), "application/json"};
  });
}