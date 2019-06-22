workspace("Shader_Livecode")
    configurations({ "Debug", "Release" })
    location("build")
    targetdir("build/")
    architecture("x64")
    includedirs({"/usr/include/SDL2/", "src/external", "src/external/imgui/", "src/external/imgui/examples"})

project("imgui")
    kind("SharedLib")
    language("C++")

    files({
        "src/external/imgui/*.cpp",
        "src/external/imgui/examples/imgui_impl_opengl3.cpp",
        "src/external/imgui/examples/imgui_impl_sdl.cpp"
    })
    
project("Main")
    kind("ConsoleApp")
    language("C++")
    targetname("livecode")
    links({"dl", "GL", "SDL2", "imgui", "lua"})
    buildoptions({"-fPIC", "-fmax-errors=5", "-std=c++17", "-Wshadow", "-Wno-literal-suffix", "-fdiagnostics-color=always"})

    files({
        "src/first.cpp"
    })

    filter("configurations:Debug")
        defines({"DEBUG", "DEVELOPER"})
        symbols("On")

    filter("configurations:Release")
        optimize("On")

    filter {}
