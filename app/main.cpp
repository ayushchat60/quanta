#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

#include "cli.h"

int main(int argc, char** argv) {
  try {
    std::vector<std::string_view> args(argv + 1, argv + argc);
    return quanta::cli::Run(args, std::cout, std::cerr);
  } catch (const std::exception& error) {
    std::cerr << "quanta: " << error.what() << '\n';
    return 1;
  }
}
