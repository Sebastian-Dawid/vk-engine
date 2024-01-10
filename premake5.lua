workspace "template"
    toolset "clang"
    cppdialect "c++20"
    configurations { "Debug", "Release" }
    location "build"

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

    project "vk-engine"
        kind "StaticLib"
        language "C++"
        targetdir "build/lib/%{cfg.buildcfg}"
        buildoptions { "-Wall", "-Werror" }

        includedirs { "include", "src/includes" }
        files { "src/**.cpp" }

    project "test-setup"
        kind "ConsoleApp"
        language "C++"
        location "tests/build"
        targetdir "tests/build/bin/%{cfg.buildcfg}"

        includedirs { "include" }
        files { "tests/setup.cpp" }
        links { "fmt", "glfw", "vulkan", "vk-engine" }
