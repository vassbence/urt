#include <sstream>
#include <urt.h>

// This AWS Lambda function expects an API Gateway event and echoes it back
// inside the body of an API Gateway response

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
  urt::run([](std::string_view event) -> std::string {
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