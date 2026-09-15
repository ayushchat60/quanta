#include "cli.h"

#include <arrow/array.h>
#include <arrow/scalar.h>
#include <arrow/util/config.h>

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_set>

#include "quanta/build_info.h"
#include "quanta/common/parse_integer.h"
#include "quanta/storage/parquet_scanner.h"

namespace quanta::cli {
namespace {
constexpr std::string_view kUsage =
    "Usage:\n  quanta version\n  quanta inspect <file.parquet>\n"
    "  quanta scan <file.parquet> [--columns a,b] [--batch-size N] [--limit N]\n"
    "Scan defaults: batch size 4096; display at most 20 rows. --limit 0 displays no rows.\n";

std::string Escape(std::string_view text) {
  std::ostringstream output;
  for (const unsigned char ch : text) {
    switch (ch) {
      case '\\':
        output << "\\\\";
        break;
      case '"':
        output << "\\\"";
        break;
      case '\n':
        output << "\\n";
        break;
      case '\r':
        output << "\\r";
        break;
      case '\t':
        output << "\\t";
        break;
      default:
        if (ch < 32 || ch == 127) {
          output << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ch)
                 << std::dec;
        } else {
          output << static_cast<char>(ch);
        }
    }
  }
  return output.str();
}

int CheckOutput(std::ostream& output, std::ostream& error) {
  if (!output) {
    error << "quanta: writing output failed\n";
    return 1;
  }
  return 0;
}

struct Arguments {
  ScanOptions scan;
  std::int64_t limit = 20;
};
arrow::Result<Arguments> ParseScan(std::span<const std::string_view> args) {
  Arguments parsed;
  std::unordered_set<std::string_view> seen;
  for (std::size_t i = 0; i < args.size(); i += 2) {
    const auto key = args[i];
    if (key != "--columns" && key != "--batch-size" && key != "--limit") {
      return arrow::Status::Invalid("Unknown scan option '", key, "'");
    }
    if (!seen.insert(key).second) return arrow::Status::Invalid("Repeated option '", key, "'");
    if (i + 1 == args.size()) return arrow::Status::Invalid("Missing value for ", key);
    const auto value = args[i + 1];
    if (key == "--columns") {
      parsed.scan.columns.emplace();
      std::size_t start = 0;
      while (true) {
        const auto comma = value.find(',', start);
        const auto name =
            value.substr(start, comma == std::string_view::npos ? comma : comma - start);
        if (name.empty()) return arrow::Status::Invalid("--columns contains an empty name");
        parsed.scan.columns->emplace_back(name);
        if (comma == std::string_view::npos) break;
        start = comma + 1;
      }
    } else {
      ARROW_ASSIGN_OR_RAISE(auto number, ParseNonnegativeInteger(value, key));
      if (key == "--limit")
        parsed.limit = number;
      else {
        if (number == 0) return arrow::Status::Invalid("--batch-size must be positive");
        parsed.scan.batch_size = number;
      }
    }
  }
  return parsed;
}

arrow::Status Inspect(const std::string& path, std::ostream& output) {
  ARROW_ASSIGN_OR_RAISE(auto scanner, ParquetScanner::Open(path));
  const auto& metadata = scanner->metadata();
  output << "File: " << Escape(metadata.path) << "\nRows: " << metadata.row_count
         << "\nColumns: " << metadata.column_count
         << "\nParquet leaf columns: " << metadata.leaf_column_count
         << "\nRow groups: " << metadata.row_groups.size() << "\nSchema:\n"
         << metadata.schema->ToString() << '\n';
  for (std::size_t i = 0; i < metadata.row_groups.size(); ++i) {
    const auto& group = metadata.row_groups[i];
    output << "Row group " << i << ": rows=" << group.row_count
           << " compressed_bytes=" << group.compressed_bytes
           << " uncompressed_bytes=" << group.uncompressed_bytes << '\n';
  }
  return arrow::Status::OK();
}

arrow::Status Scan(const std::string& path, const Arguments& args, std::ostream& output) {
  ARROW_ASSIGN_OR_RAISE(auto scanner, ParquetScanner::Open(path, args.scan));
  const auto names = args.scan.columns.value_or(scanner->metadata().schema->field_names());
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (i != 0) output << '\t';
    output << Escape(names[i]);
  }
  output << '\n';
  std::int64_t shown = 0;
  std::int64_t batches = 0;
  while (shown < args.limit) {
    ARROW_ASSIGN_OR_RAISE(auto next, scanner->Next());
    if (!next) break;
    ++batches;
    const auto& batch = *next;
    const auto count = std::min(batch.row_count(), args.limit - shown);
    for (std::int64_t row = 0; row < count; ++row) {
      for (int col = 0; col < batch.column_count(); ++col) {
        if (col != 0) output << '\t';
        ARROW_ASSIGN_OR_RAISE(auto array, batch.column(col));
        if (array->IsNull(row)) {
          output << "NULL";
        } else {
          ARROW_ASSIGN_OR_RAISE(auto scalar, array->GetScalar(row));
          const bool quote = array->type_id() == arrow::Type::STRING ||
                             array->type_id() == arrow::Type::LARGE_STRING;
          if (quote) output << '"';
          output << Escape(scalar->ToString());
          if (quote) output << '"';
        }
      }
      output << '\n';
    }
    shown += count;
  }
  output << "Displayed " << shown << (shown == 1 ? " row from " : " rows from ") << batches
         << (batches == 1 ? " batch (limit " : " batches (limit ") << args.limit << ").\n";
  return arrow::Status::OK();
}
}  // namespace

int Run(std::span<const std::string_view> args, std::ostream& output, std::ostream& error) {
  if (args.size() == 1 && (args[0] == "--help" || args[0] == "help")) {
    output << kUsage;
    return CheckOutput(output, error);
  }
  if (args.size() == 1 && args[0] == "version") {
    output << "Quanta " << QUANTA_VERSION << "\nArrow " << ARROW_VERSION_STRING << "\nCompiler "
           << QUANTA_COMPILER << "\nBuild " << QUANTA_BUILD_TYPE << '\n';
    return CheckOutput(output, error);
  }
  if (args.size() < 2 || (args[0] != "inspect" && args[0] != "scan") ||
      (args[0] == "inspect" && args.size() != 2)) {
    error << kUsage;
    return 2;
  }
  arrow::Status status;
  if (args[0] == "inspect")
    status = Inspect(std::string(args[1]), output);
  else {
    auto parsed = ParseScan(args.subspan(2));
    if (!parsed.ok()) {
      error << "quanta: " << parsed.status() << '\n';
      return 2;
    }
    status = Scan(std::string(args[1]), *parsed, output);
  }
  if (!status.ok()) {
    error << "quanta: " << status << '\n';
    return 1;
  }
  return CheckOutput(output, error);
}
}  // namespace quanta::cli
