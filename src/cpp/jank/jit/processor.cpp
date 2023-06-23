#include <cstdlib>

#include "clang/Frontend/CompilerInstance.h"

#include <jank/util/process_location.hpp>
#include <jank/util/make_array.hpp>
#include <jank/jit/processor.hpp>

namespace jank::jit
{
  option<boost::filesystem::path> find_pch()
  {
    auto const jank_path(jank::util::process_location().unwrap().parent_path());

    auto dev_path(jank_path / "CMakeFiles/jank_lib.dir/cmake_pch.hxx.pch");
    if(boost::filesystem::exists(dev_path))
    { return std::move(dev_path); }

    auto installed_path(jank_path / "../include/cpp/jank/prelude.hpp.pch");
    if(boost::filesystem::exists(installed_path))
    { return std::move(installed_path); }

    return none;
  }

  option<boost::filesystem::path> build_pch()
  {
    auto const jank_path(jank::util::process_location().unwrap().parent_path());
    auto const script_path(jank_path / "build-pch");
    auto const include_path(jank_path / "../include");
    auto const command
    (script_path.string() + " " + include_path.string() + " " + std::string{ JANK_COMPILER_FLAGS });

    std::cerr << "Note: Looks like your first run. Building pre-compiled header… " << std::flush;

    if(std::system(command.c_str()) != 0)
    {
      std::cerr << "failed to build using this script: " << script_path << std::endl;
      return none;
    }

    std::cerr << "done!" << std::endl;
    return jank_path / "../include/cpp/jank/prelude.hpp.pch";
  }

  option<boost::filesystem::path> find_llvm_resource_path()
  {
    auto const jank_path(jank::util::process_location().unwrap().parent_path());

    if(boost::filesystem::exists(jank_path / "../lib/clang"))
    { return jank_path / ".."; }

    //return JANK_CLING_BUILD_DIR;
    return none;
  }

  processor::processor()
  {
    /* TODO: Pass this into each fn below so we only do this once on startup. */
    auto const jank_path(jank::util::process_location().unwrap().parent_path());

    auto pch_path(find_pch());
    if(pch_path.is_none())
    {
      pch_path = build_pch();

      /* TODO: Better error handling. */
      if(pch_path.is_none())
      { throw std::runtime_error{ "unable to find and also unable to build PCH" }; }
    }
    auto const &pch_path_str(pch_path.unwrap().string());

    //auto const llvm_resource_path(find_llvm_resource_path());
    //if(llvm_resource_path.is_none())
    ///* TODO: Better error handling. */
    //{ throw std::runtime_error{ "unable to find LLVM resource path" }; }
    //auto const &llvm_resource_path_str(llvm_resource_path.unwrap().string());

    auto const include_path(jank_path / "../include");

    std::vector<char const*> args
    {
      //"clang++",
      "-Xclang", "-emit-llvm-only",
      "-std=gnu++17",
      "-DHAVE_CXX14=1", "-DIMMER_HAS_LIBGC=1",
      "-include-pch", pch_path_str.c_str(),
      "-isystem", include_path.c_str()

      //"-O0", "-ffast-math", "-march=native"
    };
    //interpreter = std::make_unique<clang::Interpreter>(args.size(), args.data(), llvm_resource_path_str.c_str());
    auto CI = llvm::cantFail(clang::IncrementalCompilerBuilder::create(args));
    interpreter = llvm::cantFail(clang::Interpreter::create(std::move(CI)));

    /* TODO: Optimization >0 doesn't work with the latest Cling LLVM 13.
     * 1. https://github.com/root-project/cling/issues/483
     * 2. https://github.com/root-project/cling/issues/484
     */
  }

  result<option<runtime::object_ptr>, native_string> processor::eval
  (runtime::context &, codegen::processor &cg_prc) const
  {
    /* TODO: Improve Cling to accept string_views instead. */
    //interpreter->declare(static_cast<std::string>(cg_prc.declaration_str()));
    llvm::cantFail(interpreter->ParseAndExecute(static_cast<std::string>(cg_prc.declaration_str())));

    auto const expr(cg_prc.expression_str(true));
    if(expr.empty())
    { return ok(none); }

    /* TODO: Thread safety for the rest of the JIT processor needs to be ensured. */
    static std::atomic<size_t> fn_id_counter{};
    auto const fn_id(++fn_id_counter);
    auto const fn_name(fmt::format("jank__jit__{}", fn_id));
    auto const fn_body
    (
      fmt::format
      (
        "extern \"C\" jank::runtime::object_ptr {}(){{ return {}; }}",
        fn_name,
        expr
      )
    );

    /* TODO: Undo this? */
    auto fn_result(interpreter->ParseAndExecute(fn_body));
    if(fn_result)
    { return err("compilation error"); }

    return reinterpret_cast<runtime::object_ptr (*)()>(llvm::cantFail(interpreter->getSymbolAddress(fn_name)))();
  }

  void processor::eval_string(native_string const &s) const
  { llvm::cantFail(interpreter->ParseAndExecute(static_cast<std::string>(s))); }
}
