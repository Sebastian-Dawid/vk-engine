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

    project "imgui"
        kind "StaticLib"
        language "C++"
        targetdir "build/lib/%{cfg.buildcfg}"

        includedirs { "imgui", "imgui/backends" }
        files { "imgui/*.cpp", "imgui/backends/imgui_impl_vulkan.cpp", "imgui/backends/imgui_impl_glfw.cpp" }

    project "fastgltf"
        kind "StaticLib"
        language "C++"
        targetdir "build/lib/%{cfg.buildcfg}"

        includedirs { "fastgltf/include" }
        files { "fastgltf/src/**.cpp" }

    project "vk-engine"
        kind "StaticLib"
        language "C++"
        targetdir "build/lib/%{cfg.buildcfg}"
        buildoptions { "-Wall", "-Werror" }

        filter "files:src/vk-mem-alloc/vk_mem_alloc.cpp"
            buildoptions { "-w" }
        filter {}

        includedirs { "include", "src/includes", "imgui", "fastgltf/include" }
        files { "src/**.cpp" }

    project "test-setup"
        kind "ConsoleApp"
        language "C++"
        location "tests/build"
        targetdir "tests/build/bin/%{cfg.buildcfg}"

        filter "files:**.comp or **.vert or **.frag"
            buildmessage '%{file.name}'
            buildcommands { 'mkdir -p shaders', '"glslc" -o "shaders/%{file.name}.spv" "%{file.relpath}"' }
            buildoutputs { "shaders/%{file.name}.spv" }

        filter {}

        includedirs { "include" }
        files { "tests/setup.cpp", "tests/shaders/**" }
        links { "fmt", "glfw", "vulkan", "vk-engine", "imgui", "fastgltf" }
