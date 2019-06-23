extern "C" {

    #include "GL/gl3w.h"
    #include "GL/gl3w.c"
    #undef ARRAY_SIZE // NOTE(justas): gl3w defines this
    
    #include <GL/gl.h>
}

//#include "external/sol/forward.hpp"
#include "external/sol/sol.hpp"

#include "stormy.cpp"
#include "SDL2/SDL.h"
#include <chrono>
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

intern auto base_untracked_malloc_allocator = make_malloc_memory_allocator();
intern auto malloc_allocator = make_tracked_memory_allocator(&base_untracked_malloc_allocator);

intern
b32 compile_shader_part(
        GLenum type, 
        String source,
        String defines,
        Memory_Allocator * temp_alloc,
        String * out_error,
        u32 * out_id
) {
    auto id = glCreateShader(type);

    if(id == 0) {
        *out_error = "failed to allocate shader id in make_gl_shader"_S;
        return false;
    }

    baked auto shader_preprocessing = "#version 330 core\n"_S;

    const char * sources[] = {
        shader_preprocessing.str,
        defines.str,
        source.str
    };

    s32 lengths[] = {
        (s32)shader_preprocessing.length,
        (s32)defines.length,
        (s32)source.length,
    };
    
    assert(ARRAY_SIZE(lengths) == ARRAY_SIZE(sources));

    glShaderSource(id, ARRAY_SIZE(sources), sources, lengths);
    glCompileShader(id);

    {
        s32 status;
        glGetShaderiv(id, GL_COMPILE_STATUS, &status);

        if(status != GL_TRUE) {

            s32 log_length = 0;
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_length);

            if(log_length != 0) {
                *out_error = allocate_temp_string(temp_alloc, log_length, "gl_make_shader log_buffer");
                glGetShaderInfoLog(id, log_length, &log_length, (char*)out_error->str);

            } else {
                *out_error = "failed to compile shader but no logs were provided."_S;
            }

            glDeleteShader(id);

            return false;
        }
    }

    *out_id = id;
    return true;
}

struct Gl_Shader_Part {
    String name = empty_string;
    u64 comparison_hash;
    u32 id = -1;
    b32 did_change_this_frame;
};

struct Gl_Shader {
    String name = empty_string;
    u32 id = -1;

    Array<u64> part_hashes;

    Gl_Shader() {
        part_hashes = make_array<u64>(8, &malloc_allocator, "gl shader part hashes"_S);
    }

    void free() {
        array_free(&part_hashes);
    }
};

struct Asset_Entry {
    const char * path = 0;
    b32 needs_first_load = true;
    u64 last_load_time = 0;
    f64 countdown_to_load = 0;
};

struct Lua_Renderer {
    f64 target_fps = 60.0;

    Memory_Allocator alloc;
    Memory_Allocator * temp_alloc;

    Table<Asset_Entry> asset_catalogue;
    Table<Gl_Shader_Part> shader_parts;
    Table<Gl_Shader> shaders;

    sol::state lua;

    Gl_Shader active_shader;
    b32 can_render;
    b32 needs_free;

    force_inline
    Asset_Entry * get_asset(u64 hash, const char * path) {
        auto * asset = table_insert_or_initialize_new(&asset_catalogue, hash);
        asset->path = path;
        return asset;
    }

    void free() {
        For(shaders) {
            glDeleteProgram(it->value.id);
            it->value.free();
        }

        free_tracked_memory_allocator(&alloc);

        asset_catalogue = {};
        shader_parts = {};
        shaders = {};
    }
};

intern Lua_Renderer renderer;
intern f64 dt;

intern
b32 does_asset_need_loading(Asset_Entry * asset) {
    auto ret = false;
    if(asset->needs_first_load) {
        asset->last_load_time = plat_get_file_modification_time(asset->path);
        asset->needs_first_load = false;
        ret = true;
    }
    else if(asset->countdown_to_load > 0) {
        asset->countdown_to_load -= dt;
        if(asset->countdown_to_load <= 0) {
            ret = true;
            asset->countdown_to_load = -1;
        }
    }
    else {
        auto time = plat_get_file_modification_time(asset->path);

        if(time != asset->last_load_time) {
            asset->last_load_time = time;
            asset->countdown_to_load = .2;
        }
    }

    return ret;
}

intern force_inline
Gl_Shader_Part make_gl_shader(String name) {
    Gl_Shader_Part ret = {};
    ret.name = name;
    ret.id = glCreateProgram();
    return ret;
}

intern
b32 did_shader_compile_properly(
        u32 id,
        Memory_Allocator * temp_alloc,
        String * out_error
) {
    s32 status;
    glGetProgramiv(id, GL_LINK_STATUS, &status);

    if(status == GL_TRUE) {
        return true;
    }

    s32 log_size;
    glGetProgramiv(id, GL_INFO_LOG_LENGTH, &log_size);

    if(log_size == 0) {
        *out_error = "link failed but no log was provided."_S;
        return false;
    }

    *out_error = allocate_temp_string(temp_alloc, log_size, "gl_verify_program_link log_buffer");
    glGetProgramInfoLog(id, log_size, &log_size, (char*)out_error->str);

    return false;
}

intern
s32 get_uniform_index(
        Gl_Shader * shader,
        const char * loc,
        b32 complain = true
) {
    auto val = glGetUniformLocation(shader->id, loc);
    if(complain && val == -1) {
        //printf("failed to find uniform '%s' in program '%.*s'\n", loc, shader->name.length, shader->name.str);
    }

    return val;
}

intern force_inline
void set_uniform_f32(Gl_Shader * shader, const char * name, f64 value) {
    glUniform1f(get_uniform_index(shader, name), value);
}

intern force_inline
void set_uniform_b32(Gl_Shader * shader, const char * name, b32 value) {
    glUniform1i(get_uniform_index(shader, name), value);
}

intern force_inline
void set_uniform_v2_f32(Gl_Shader * shader, const char * name, v2_f64 value, b32 complain = true) {
    v2 converted = v2_f64_to_v2_f32(value);
    glUniform2fv(get_uniform_index(shader, name, complain), 1, (f32*)&converted);
}

intern force_inline
void set_uniform_v4_f32(Gl_Shader * shader, const char * name, v4_f64 value) {
    v4 converted = v4_f64_to_v4_f32(value);
    glUniform4fv(get_uniform_index(shader, name), 1, (f32*)&converted);
}

intern force_inline
void set_uniform_v2_s32(Gl_Shader * shader, const char * name, v2_s32 value) {
    glUniform2iv(get_uniform_index(shader, name), 1, (s32*)&value);
}

intern force_inline
void set_uniform_s32(Gl_Shader * shader, const char * name, s32 value) {
    glUniform1i(get_uniform_index(shader, name), value);
}

intern force_inline
void set_uniform_v2_f32(Gl_Shader * shader, const char * name, v2 value, b32 complain = true) {
    glUniform2fv(get_uniform_index(shader, name, complain), 1, (f32*)&value);
}


intern force_inline
void set_uniform_v3_f32(Gl_Shader * shader, const char * name, v3 value) {
    glUniform4fv(get_uniform_index(shader, name), 1, (f32*)&value);
}

intern force_inline
void set_uniform_v4_f32(Gl_Shader * shader, const char * name, v4 value) {
    glUniform4fv(get_uniform_index(shader, name), 1, (f32*)&value);
}

intern force_inline
void set_uniform_m3_f64(Gl_Shader * shader, const char * name, m3_f64 value) {
    m3 converted = m3_f64_to_m3_f32(&value);
    glUniformMatrix3fv(get_uniform_index(shader, name), 1, GL_FALSE, (f32*)&converted);
}

intern force_inline
void set_uniform_m4(Gl_Shader * shader, const char * name, m4 value) {
    glUniformMatrix4fv(get_uniform_index(shader, name), 1, GL_FALSE, (f32*)&value);
}

intern u32 quad_vao;
intern u32 quad_vbo;

intern auto temp_allocator = make_arena_memory_allocator(m_new(&malloc_allocator, MEGABYTES(16)));
intern f32 shader_time =0;
intern v2 mouse_pos = {};
intern b32 is_lmb_down = false;
intern b32 is_rmb_down = false;
intern v2 window_size;

intern
void process_button_state(s32 button, b32 state) {
    if(button == SDL_BUTTON_LEFT) is_lmb_down = state;
    else if(button == SDL_BUTTON_RIGHT) is_rmb_down = state;
}

intern 
b32 try_load_renderer(
        const char * script_dir,
        Lua_Renderer * out_renderer,
        String * out_error
        
) {
    auto script_read = plat_fs_read_entire_file(script_dir, &temp_allocator);
    if(!script_read.did_succeed) {
        *out_error = "failed to read lua script\n"_S;
        return false;
    }

    auto cstring_script = temp_cstring(script_read.as_string, &temp_allocator);

    sol::state temp_lua;

    auto script_result = temp_lua.script(cstring_script, [](lua_State*, sol::protected_function_result pfr) {
        return pfr;

    }, "render script");

    if(!script_result.valid()) {
        sol::error e = script_result;
        *out_error = make_string(e.what());
        return false;
    }

    *out_renderer = {};
    auto & our_rend = *out_renderer;
    our_rend.temp_alloc = &temp_allocator;
    our_rend.alloc = make_tracked_memory_allocator(&base_untracked_malloc_allocator);
    our_rend.asset_catalogue = make_table<Asset_Entry>(8, &our_rend.alloc, "asset catalogue"_S);
    our_rend.shader_parts = make_table<Gl_Shader_Part>(8, &our_rend.alloc, "shader parts"_S);
    our_rend.shaders = make_table<Gl_Shader>(8, &our_rend.alloc, "shaders"_S);
    our_rend.lua = std::move(temp_lua);
    our_rend.needs_free = true;
    our_rend.can_render = true;

    auto & lua = our_rend.lua;

    lua.open_libraries(sol::lib::base);
    lua["GL_VERTEX"] = GL_VERTEX_SHADER;
    lua["GL_FRAGMENT"] = GL_FRAGMENT_SHADER;

    lua["gl_set_default_uniforms"] = [](Lua_Renderer * r) {
        set_uniform_f32(&r->active_shader, "iTime", shader_time);
        set_uniform_v2_f32(&r->active_shader, "iResolution", window_size);

        static v4 mouse = {};
        mouse.z = (f32)is_lmb_down;
        mouse.w = (f32)is_rmb_down;

        if(is_lmb_down) {
            mouse.x = mouse_pos.x;
            mouse.y = mouse_pos.y;
        }

        set_uniform_v4_f32(&r->active_shader, "iMouse", mouse);
    };

    lua["gl_load_shader_part"] = [](Lua_Renderer * r, const char * cname, s32 type, const char * dir) {
        auto name = make_string(cname);
        auto hash = hash_fnv(name);

        auto * asset = r->get_asset(hash, dir);
        auto * part = table_insert_or_initialize_new(&r->shader_parts, hash);

        auto load = false;
        if(does_asset_need_loading(asset)) {
            load = true;
        }
        else if(!string_equals_case_sensitive(name, part->name)) {
            load = true;
        }

        if(load) {
            auto read = plat_fs_read_entire_file(dir, r->temp_alloc);

            if(!read.did_succeed) {
                printf("failed to read shader %s\n", dir);
                return (void*)0;
            }

            String error;
            u32 part_id;
            auto result = compile_shader_part(type, read.as_string, empty_string, r->temp_alloc, &error, &part_id);
            if(!result) {
                printf("failed to compile shader '%s': %.*s\n", cname, error.length, error.str);
                return (void*)0;
            }

            if(part->id != -1) {
                glDeleteProgram(part->id);
            }

            printf("compiled new shader '%s' %d %d %llu\n", cname, type, part_id, hash);

            part->name = name;
            part->id = part_id;
            part->did_change_this_frame = true;
            part->comparison_hash = hash * 13 + asset->last_load_time;
        }

        return (void*)hash;
    };

    lua["gl_use_shader"] = [](Lua_Renderer * r, void * void_shader_hash) {
        auto shader_hash = (u64)void_shader_hash;
        auto * shader = table_insert_or_initialize_new(&r->shaders, shader_hash);

        if(shader->id == -1) {
            return;
        }

        glUseProgram(shader->id);
        r->active_shader = *shader;
    };

    lua["target_fps"] = [](Lua_Renderer * r, f32 target_fps) {
        r->target_fps = target_fps;
    };

    lua["gl_load_shader"] = [](Lua_Renderer * r, const char * cname, sol::table t) {

        auto name = make_string(cname);
        auto hash = hash_fnv(name);

        auto * shader = table_insert_or_initialize_new(&r->shaders, hash);

        auto needs_reload = false;

        auto has_hash_mismatch = false;
        auto num_parts = 0;
        for(auto & kvp : t) {
            num_parts++;

            auto part_hash = (u64)kvp.second.as<void*>();
            auto * part = table_insert_or_initialize_new(&r->shader_parts, part_hash);

            auto does_have_this_hash = false;
            for(auto * it : shader->part_hashes) {
                if(*it == part->comparison_hash) {
                    does_have_this_hash = true;
                    break;
                }
            }

            if(!does_have_this_hash) {
                has_hash_mismatch = true;
                break;
            }
        }

        if(has_hash_mismatch) {
            needs_reload = true;
        }
        else if(shader->part_hashes.watermark <= 0 && num_parts > 0) {
            needs_reload = true;
        }

        if(needs_reload) {
            array_clear(&shader->part_hashes);

            auto id = glCreateProgram();

            printf("reloading shader %s\n", cname);

            if(shader->id != -1) {
                glDeleteProgram(shader->id);
            }

            for(auto & kvp : t) {
                auto part_hash = (u64)kvp.second.as<void*>();
                auto * part = table_insert_or_initialize_new(&r->shader_parts, part_hash);
                *array_append(&shader->part_hashes) = part->comparison_hash;

                if(part->id == -1) {
                    printf("gl_load_shader was passed an uninitialized shader %llu!\n", part_hash);
                    glDeleteProgram(shader->id);
                    return (void*)0;
                }

                glAttachShader(id, part->id);
            }
            glLinkProgram(id);

            String error;
            if(!did_shader_compile_properly(id, &temp_allocator, &error)) {
                printf("Failed to compile shader '%s'. Error:\n", cname);
                printf("%.*s\n", error.length, error.str);

                glDeleteProgram(id);
                return (void*)0;
            }

            shader->id = id;
            shader->name = name;
        }
        return (void*)hash;
    };

    lua["gl_enable_alpha_blend"] = [](Lua_Renderer * r) {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    };
    lua["gl_disable_alpha_blend"] = [](Lua_Renderer * r) {
        glDisable(GL_BLEND);
    };

    lua["gl_clear_color"] = [](Lua_Renderer * r) {
        glClear(GL_COLOR_BUFFER_BIT);
    };

    lua["gl_default_viewport"] = [](Lua_Renderer * r) {
        glViewport(0,0,window_size.x, window_size.y);
    };

    lua["gl_default_fb"] = [](Lua_Renderer * r ) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    };

    lua["gl_draw_quad"] = [](Lua_Renderer * r) {
        glBindVertexArray(quad_vao);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    };

    lua["gl_uniform_f32"] = [](Lua_Renderer * r, const char * uniform, f32 num) {
        set_uniform_f32(&r->active_shader, uniform, num);
    };

    lua["gl_uniform_v2_f32"] = [](Lua_Renderer * r, const char * uniform, f32 x, f32 y) {
        v2 v = {x,y};
        set_uniform_v2_f32(&r->active_shader, uniform, v);
    };

    return true;
}

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("usage: %s <loop lua file> \n", argv[0]);
        return 1;
    }

    window_size = make_vector(1280, 720);
    SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);

    auto window = SDL_CreateWindow("shader-env", 
            SDL_WINDOWPOS_UNDEFINED, 
            SDL_WINDOWPOS_UNDEFINED,
            window_size.x, window_size.y,
            SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

    auto gl_context = SDL_GL_CreateContext(window);

    gl3wInit();
    
    glClearColor(8.f/255.f, 7.f/255.f, 17.f/255.f, 1.0f);

    SDL_GL_SetSwapInterval(1);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(0);

    glGenVertexArrays(1, &quad_vao);
    glBindVertexArray(quad_vao);

    glGenBuffers(1, &quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);

    f32 quadVerts[] = {
        -1.0f, -1.0f,
        -1.0f, 1.0f,
        1.0f, -1.0f,
        1.0f, 1.0f
    };

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    b32 should_quit = false;

    dt = 1.0f/60.f;

    auto script_file_dir = argv[1];

    Asset_Entry script_asset = {};
    script_asset.path = script_file_dir;

    sol::state lua_state; 

    while(!should_quit) {
        auto startT = std::chrono::high_resolution_clock::now();

        memory_allocator_arena_reset(&temp_allocator);
        shader_time += dt;

        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if(event.type == SDL_QUIT) {
                should_quit = true;
            }
            else if(event.type == SDL_MOUSEMOTION) {
                mouse_pos.x = event.motion.x;
                mouse_pos.y = event.motion.y;
            }
            else if(event.type == SDL_MOUSEBUTTONDOWN) {
                process_button_state(event.button.button, true);
            }
            else if(event.type == SDL_MOUSEBUTTONUP) {
                process_button_state(event.button.button, false);
            }
            else if(event.type == SDL_KEYDOWN) {
                switch(event.key.keysym.sym) {
                    case SDLK_q: {
                        should_quit = true;
                        break;
                    }
                }
            }
            else if(event.type == SDL_WINDOWEVENT)
            {
                if(event.window.event == SDL_WINDOWEVENT_RESIZED || 
                  event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {

                    window_size.x = event.window.data1;
                    window_size.y = event.window.data2;
                    glViewport(0,0,window_size.x, window_size.y);
                }
            }
        }

        if(does_asset_need_loading(&script_asset)) {

            String error;
            Lua_Renderer temp;
            if(!try_load_renderer(script_asset.path, &temp, &error)) {
                printf("[renderer] load error: %.*s\n", error.length, error.str);
            }
            else {
                printf("reloaded renderer\n");
                if(renderer.needs_free) {
                    renderer.free();
                }
                renderer = std::move(temp);
            }
        }
    
        if(renderer.can_render) {
            auto & lua = renderer.lua;

            lua["dt"] = dt;
            lua["r"] = &renderer;
            lua["_time"] = shader_time;
            lua["_resolution_x"] = (f32)window_size.x;
            lua["_resolution_y"] = (f32)window_size.y;

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame(window);
            ImGui::NewFrame();

            {
                sol::protected_function render_fx = lua["render"];
                auto result = render_fx();

                if(!result.valid()) {
                    sol::error e = result;
                    printf("rendering failed: %s\n", e.what());
                    renderer.can_render = false;
                }
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        SDL_GL_SwapWindow(window);

        auto delta = std::chrono::duration<f64>(std::chrono::high_resolution_clock::now() - startT).count();
        auto target_delta = 1.0 / renderer.target_fps;

        if(target_delta > delta) {
            auto wait_time = target_delta - delta;
            plat_sleep(wait_time);
            dt = target_delta;
        }
        else {
            dt = delta;
        }
    }
}
