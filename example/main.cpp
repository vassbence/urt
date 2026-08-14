#include <sstream>
#include <urt.h>

int main() {
  urt::run([](std::string_view event) {
    std::ostringstream response;
    response << "{\n"
             << "\"statusCode\": 200,\n"
             << "\"headers\": { \"Content-Type\": \"application/json\" },\n"
             << "\"isBase64Encoded\": false,\n"
             << "\"body\": \"" << event << "\"\n"
             << "}";
    return response.str();
  });
}