# µrt

µrt is a custom [AWS Lambda](https://en.wikipedia.org/wiki/AWS_Lambda) runtime for C++ applications.

## Features

- Single-digit millisecond cold starts, no dynamic memory allocations
- One header file, no third-party dependencies

## Installation

Add the [header file](/include/urt) to your project by hand or install via [CMake](https://cmake.org).

```cmake
cmake_minimum_required(VERSION 3.22.2)
project(example)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
        urt
        GIT_REPOSITORY https://github.com/vassbence/urt.git
        GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(urt)

add_executable(${PROJECT_NAME} "main.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE urt)
```

## Usage

```c++
#include <sstream>
#include <urt.h>

int main() {
  urt::run([](std::string_view event) {
    std::ostringstream response;
    response << "{\n"
             << "  \"statusCode\": 200,\n"
             << "  \"headers\": { \"Content-Type\": \"application/json\" },\n"
             << "  \"isBase64Encoded\": false,\n"
             << "  \"body\": \"" << event << "\"\n"
             << "}";
    return response.str();
  });
}
```

See the [example folder](/example) in the repository for a complete implementation.

## Deployment to AWS

Follow the [official deployment guide](https://docs.aws.amazon.com/lambda/latest/dg/getting-started.html) from AWS. Use the `provided.al2023` runtime and name your compiled executable `bootstrap`.

## License

MIT license
