add_rules("mode.debug", "mode.release", "mode.coverage")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "build/" })

set_toolchains("llvm")
add_cxflags("-Wno-pre-c++20-compat", "-Wno-pre-c++17-compat", "-Wno-c++98-compat", "-Wno-c++98-compat-pedantic")
add_cxflags("-Wno-reserved-identifier")

set_languages("c++2c")
set_warnings("everything", "error")
add_cxxflags("-fexperimental-library")
if not is_plat("windows") then
    add_syslinks("c++abi")
end

set_runtimes("c++_static")

if is_mode("coverage") then
    add_cxxflags("-fprofile-instr-generate", "-fcoverage-mapping")
    add_ldflags("-fprofile-instr-generate")
end

if is_mode("ALUSan") then
    set_symbols("debug")
    set_optimize("fast") 
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.leak", true)
    set_policy("build.sanitizer.undefined", true)
    add_cxflags("-fno-omit-frame-pointer -fno-optimize-sibling-calls")
end

-- TODO(metrapoliten): MSan fails on ut library: https://github.com/boost-ext/ut/issues/712.
-- I couldn't fix or ignore the error. It might be worth switching to another library later.
--[[ 
if is_mode("MSan") then
    set_symbols("debug")
    set_optimize("fast") 
    set_policy("build.sanitizer.memory", true)
    add_cxflags("-fsanitize-memory-track-origins")
    add_cxflags("-fno-omit-frame-pointer -fno-optimize-sibling-calls")
    add_cxflags("-fsanitize-ignorelist=" .. path.join(os.projectdir(), "msan_ignore"), {force = true})
    add_ldflags("-fsanitize-ignorelist=" .. path.join(os.projectdir(), "msan_ignore"))
end
]]

if is_mode("TSan") then
    set_symbols("debug")
    set_optimize("fast") 
    set_policy("build.sanitizer.thread", true)
    add_cxflags("-fno-omit-frame-pointer -fno-optimize-sibling-calls")
end

option("hardening")
    set_default(false)
    set_showmenu(true)
    set_description("Set libc++ debug hardening mode. For details: https://libcxx.llvm.org/Hardening.html")
option_end()

if has_config("hardening") then 
    add_defines("_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG")
end

package("taskflow")
    set_kind("library", {headeronly = true})
    set_urls("https://github.com/taskflow/taskflow/archive/refs/tags/$(version).tar.gz")
    add_versions("v4.0.0", "a9d27ad29caffc95e394976c6a362debb94194f9b3fbb7f25e34aaf54272f497")

    on_install(function (package)
        io.gsub("taskflow/taskflow.hpp", "^(.-%s+.-%s+)", "%1#include <bit>\n")
        os.cp("taskflow", package:installdir("include"))
    end)

    if is_plat("linux") then
        add_syslinks("pthread")
    end
package_end()

add_requires("taskflow")

target("conv")
    set_kind("static")
    add_sysincludedirs("deps", { private = true })
    add_cxxflags("-Wno-padded")
    add_packages("taskflow")
    add_files("src/*.cc")
    add_files("src/*.ccm", { public = true })

target("ut")
    set_kind("static")
    set_default(false)
    set_warnings("none")
    add_sysincludedirs("deps/ut", { private = true})
    add_files("deps/ut/ut.cppm", { public = true })

target("test")
    set_kind("binary")
    set_default(false)
    add_deps("conv", "ut")
    add_tests("default")
    add_files("test/test.cc")
    
 target("cifuzz")
    set_kind("binary")
    set_default(false)
    set_enabled(is_mode("cifuzz"))        
    add_deps("conv")
    add_files("test/fuzz.cc")
