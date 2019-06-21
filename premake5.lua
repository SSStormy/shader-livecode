workspace("Shader_Livecode")
    configurations({ "Debug", "Release" })
    location("build")
    targetdir("build/")
    architecture("x64")

project("imgui")
    kind("SharedLib")
    language("C++")

    files({
        "src/external/imgui/*.cpp"
    })
    
project("Main")
    kind("ConsoleApp")
    language("C++")
    targetname("livecode")
    links({"dl", "GL", "SDL2", "imgui"})
    buildoptions({"-fPIC", "-fmax-errors=5", "-std=c++14", "-Wshadow", "-Wno-literal-suffix", "-fdiagnostics-color=always"})

    files({
        "src/first.cpp"
    })

    filter("configurations:Debug")
        defines({"DEBUG", "DEVELOPER"})
        symbols("On")

    filter("configurations:Release")
        optimize("On")

    filter {}
