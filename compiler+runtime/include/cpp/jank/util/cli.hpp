#pragma once

#include <jtl/result.hpp>

namespace jank::util::cli
{
  enum class command : u8
  {
    run,
    compile,
    compile_module,
    repl,
    cpp_repl,
    run_main
  };

  struct options
  {
    /* Runtime. */
    native_transient_string module_path;
    bool profiler_enabled{};
    native_transient_string profiler_file{ "jank.profile" };
    bool gc_incremental{};

    /* Native dependencies. */
    native_vector<jtl::immutable_string> include_dirs;
    native_vector<jtl::immutable_string> library_dirs;
    native_vector<jtl::immutable_string> define_macros;
    native_vector<jtl::immutable_string> libs;

    /* Compilation. */
    i64 optimization_level{};

    /* Run command. */
    native_transient_string target_file;

    /* Compile command. */
    native_transient_string target_module;
    native_transient_string target_runtime{ "dynamic" };

    /* REPL command. */
    bool repl_server{};

    /* Extras.
     * TODO: Use a native_persistent_vector instead.
     * */
    std::vector<native_transient_string> extra_opts;

    native_transient_string output_filename;

    command command{ command::repl };
  };

  jtl::result<options, int> parse(int const argc, char const **argv);
  std::vector<native_transient_string> parse_empty(int const argc, char const **argv);
}
