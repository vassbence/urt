#include <sstream>
#include <urt.h>

std::string escape_json(std::string_view value) {
  std::string result;
  for (char c : value) {
    if (c == '"') {
      result += "\\\"";
    } else if (c == '\n') {
      result += "\\n";
    } else if (c == '\r') {
      result += "\\r";
    } else if (c == '\\') {
      result += "\\\\";
    } else {
      result += c;
    }
  }
  return result;
}

int main() {
  urt::run([](std::string_view event) {
    std::ostringstream response;
    response << "{\n"
             << "\"statusCode\": 200,\n"
             << "\"headers\": { \"Content-Type\": \"application/json\" },\n"
             << "\"isBase64Encoded\": false,\n"
             << "\"body\": \"" << escape_json(event) << "\"\n"
             << "}";
    return response.str();
  });
}