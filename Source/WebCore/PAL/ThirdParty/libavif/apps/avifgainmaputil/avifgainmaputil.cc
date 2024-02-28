// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <iomanip>
#include <string>
#include <vector>

#include "argparse.hpp"
#include "avif/avif.h"
#include "avifutil.h"
#include "combine_command.h"
#include "convert_command.h"
#include "extractgainmap_command.h"
#include "printmetadata_command.h"
#include "program_command.h"
#include "swapbase_command.h"
#include "tonemap_command.h"

namespace avif {
namespace {

class HelpCommand : public ProgramCommand {
 public:
  HelpCommand() : ProgramCommand("help", "Prints a command's usage") {}
  avifResult Run() override {
    // Actual implementation is in the main function because it needs access
    // to the list of commands.
    return AVIF_RESULT_OK;
  }
};

void PrintUsage(const std::vector<std::unique_ptr<ProgramCommand>>& commands) {
  std::cout << "\nExperimental tool to manipulate avif images with HDR gain "
               "maps.\n\n";
  std::cout << "usage: avifgainmaputil <command> [options] [arguments...]\n\n";
  std::cout << "Available commands:\n";
  int longest_command_size = 0;
  for (const std::unique_ptr<ProgramCommand>& command : commands) {
    longest_command_size = std::max(longest_command_size,
                                    static_cast<int>(command->name().size()));
  }
  for (const std::unique_ptr<ProgramCommand>& command : commands) {
    std::cout << "  " << std::left << std::setw(longest_command_size)
              << command->name() << "  " << command->description() << "\n";
  }
  std::cout << "\n";
  avifPrintVersions();
}

}  // namespace
}  // namespace avif

int main(int argc, char** argv) {
  std::vector<std::unique_ptr<avif::ProgramCommand>> commands;
  commands.emplace_back(std::make_unique<avif::HelpCommand>());
  commands.emplace_back(std::make_unique<avif::CombineCommand>());
  commands.emplace_back(std::make_unique<avif::ConvertCommand>());
  commands.emplace_back(std::make_unique<avif::TonemapCommand>());
  commands.emplace_back(std::make_unique<avif::SwapBaseCommand>());
  commands.emplace_back(std::make_unique<avif::ExtractGainMapCommand>());
  commands.emplace_back(std::make_unique<avif::PrintMetadataCommand>());

  if (argc < 2) {
    std::cerr << "Command name missing\n";
    avif::PrintUsage(commands);
    return 1;
  }

  const std::string command_name(argv[1]);
  if (command_name == "help") {
    if (argc >= 3) {
      const std::string sub_command_name(argv[2]);
      for (const auto& command : commands) {
        if (command->name() == sub_command_name) {
          command->PrintUsage();
          return 0;
        }
      }
      std::cerr << "Unknown command " << sub_command_name << "\n";
      avif::PrintUsage(commands);
      return 1;
    } else {
      avif::PrintUsage(commands);
      return 0;
    }
  }

  for (const auto& command : commands) {
    if (command->name() == command_name) {
      try {
        avifResult result = command->ParseArgs(argc - 1, argv + 1);
        if (result == AVIF_RESULT_OK) {
          result = command->Run();
        }

        if (result == AVIF_RESULT_INVALID_ARGUMENT) {
          command->PrintUsage();
        }
        return (result == AVIF_RESULT_OK) ? 0 : 1;
      } catch (const std::invalid_argument& e) {
        std::cerr << e.what() << "\n\n";
        command->PrintUsage();
        return 1;
      }
    }
  }

  std::cerr << "Unknown command " << command_name << "\n";
  avif::PrintUsage(commands);
  return 1;
}
