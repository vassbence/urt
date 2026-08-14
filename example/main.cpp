#include <format>
#include <urt.h>

int main() {
  urt::run([](std::string_view event) {
    return std::format(
        R"({{ "statusCode": 200, "headers": {{ "Content-Type": "application/json" }}, "isBase64Encoded": false, "body": "{}" }})",
        event);
  });
}