#include "include/clara.hpp"
#include "include/drv-loader.hpp"

#include <iostream>

#define NOMINMAX
#include <windows.h>

static std::uint32_t print_last_error(void) {
    std::uint32_t last_error_code = ::GetLastError();
    std::cerr << "[!] Failed with last error " << last_error_code << std::endl;

    return last_error_code;
}

static void banner(void) {
    std::cout <<
        "      _                   _                 _           \n"
        "     | |                 | |               | |          \n"
        "   __| |_ ____   __      | | ___   __ _  __| | ___ _ __ \n"
        "  / _` | '__\\ \\ / /______| |/ _ \\ / _` |/ _` |/ _ \\ '__|\n"
        " | (_| | |   \\ V //_____/| | (_) | (_| | (_| |  __/ |   \n"
        "  \\__,_|_|    \\_/        |_|\\___/ \\__,_|\\__,_|\\___|_|   \n"
        "                                                        \n"
        "   (c) Midi12                                           \n"
        << std::endl;
}

int main(int argc, char* argv[], char* envp[]) {
    bool show_help = false;
    drv_loader::config_t config = {};

    auto cmd_parser = clara::Help(show_help)
        | clara::Opt(
            [&](const std::string& display_name) { config.display_name = display_name; },
            "Display name"
        )["--display"]["-d"]("Set the display name").required()
        | clara::Opt(
            [&](const std::string& op_type) {
                auto ret = clara::ParserResult::runtimeError("Unrecognized operation");

                if (op_type == "load") {
                    config.operation = drv_loader::loader_operation_t::load;
                    ret = clara::ParserResult::ok(clara::ParseResultType::Matched);
                } else if (op_type == "unload") {
                    config.operation = drv_loader::loader_operation_t::unload;
                    ret = clara::ParserResult::ok(clara::ParseResultType::Matched);
                }

                return ret;
            },
            "load|unload"
        )["--operation"]["-o"]("Load or unload the specified driver").required()
        | clara::Arg(
            [&](const std::string& file_path) { config.file_path = file_path; },
            "Driver file path"
        )("Driver file to load");

    auto result = cmd_parser.parse(clara::detail::Args(argc, argv));

    banner();

    if (!result) {
        std::cerr << "[!] Error in command line: " << result.errorMessage() << std::endl;
        return 1;
    }

    if (show_help) {
        std::cout << cmd_parser << std::endl;
        return 0;
    }

    if (config.operation == drv_loader::loader_operation_t::load && config.file_path.empty()) {
        std::cerr << "[!] File path is empty" << std::endl;
        return 1;
    }

    if (config.display_name.empty()) {
        std::cerr << "[!] Display name is empty" << std::endl;
        return 1;
    }

    if (!helpers::add_privilege("SeLoadDriverPrivilege")) {
        std::cerr << "[!] Failed to add SeLoadDriverPrivilege privilege" << std::endl;
        return 1;
    }

    std::uint32_t ret = drv_loader::load_unload(config);

    if (ret != ERROR_SUCCESS) {
        std::cerr << "[!] An error happened (last error: " << helpers::to_hex_string(ret) << ")" << std::endl;
    } else {
        switch (config.operation) {
            case drv_loader::loader_operation_t::load:
                std::cout << "[+] Driver loaded successfully" << std::endl;
                break;
            case drv_loader::loader_operation_t::unload:
                std::cout << "[+] Driver unloaded successfully" << std::endl;
                break;
            default:
                break;
        }
    }

    return 0;
}