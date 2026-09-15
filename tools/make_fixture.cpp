#include <exception>
#include <iostream>

#include "parquet_fixture.h"
#include "quanta/common/parse_integer.h"

int main(int argc, char** argv) {
  if (argc < 2 || argc > 4) {
    std::cerr << "Usage: quanta_make_fixture <output.parquet> [rows=23] [row-group-size=7]\n";
    return 2;
  }
  try {
    auto rows = quanta::ParseNonnegativeInteger(argc > 2 ? argv[2] : "23", "rows");
    auto groups = quanta::ParseNonnegativeInteger(argc > 3 ? argv[3] : "7", "row-group-size");
    if (!rows.ok() || !groups.ok() || *groups == 0) {
      std::cerr << "Rows must be nonnegative and row-group-size positive 64-bit integers\n";
      return 2;
    }
    if (std::filesystem::exists(argv[1])) {
      std::cerr << "Refusing to overwrite existing file: " << argv[1] << '\n';
      return 1;
    }
    auto status = quanta::testing::WriteFixture(argv[1], *rows, *groups);
    if (!status.ok()) {
      std::cerr << status << '\n';
      return 1;
    }
    std::cout << "Wrote " << *rows << " rows to " << argv[1] << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
