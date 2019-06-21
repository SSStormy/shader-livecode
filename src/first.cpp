extern "C" {

    #include "external/gl3w/gl3w.h"
    #include "external/gl3w/gl3w.c"
    #undef ARRAY_SIZE // NOTE(justas): gl3w defines this
    
    #include <GL/gl.h>
}

#include "stormy.cpp"
#include "SDL2/SDL.h"
#include <chrono>

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

struct Gl_Shader {
    String name;
    u32 id;
};

intern force_inline
Gl_Shader make_gl_shader(String name) {
    Gl_Shader ret = {};
    ret.name = name;
    ret.id = glCreateProgram();
    return ret;
}

intern force_inline
b32 link_shader(
        Gl_Shader * shader,
        Memory_Allocator * temp_alloc,
        String * out_error
) {
    auto id = shader->id;

    glLinkProgram(id);

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
s32 get_uniform_loc(u32 program, const char* loc, const char *program_name) {
    s32 val = glGetUniformLocation(program, loc);
    if(val == -1) {
        printf("failed to find uniform '%s' in program '%s'\n", loc, program_name);
    }
    return val;
}

struct program_uniform {
    u32 id;
    const char * name;
};

intern
program_uniform make_immediate_uniform(const char * name) {
    program_uniform ret;

    ret.name = name;
    ret.id = 0;

    return ret;
}

intern
s32 get_uniform_index(
        Gl_Shader * shader,
        const char * loc,
        b32 complain = true
) {
    auto val = glGetUniformLocation(shader->id, loc);
    if(complain && val == -1) {
        printf("failed to find uniform '%s' in program '%.*s'\n", loc, shader->name.length, shader->name.str);
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
void set_uniform_v2_f32(Gl_Shader * shader, const char * name, v2 value, b32 complain = true) {
    glUniform2fv(get_uniform_index(shader, name, complain), 1, (f32*)&value);
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
void set_uniform_v4_f32(Gl_Shader * shader, const char * name, v4_f64 value) {
    v4 converted = v4_f64_to_v4_f32(value);
    glUniform4fv(get_uniform_index(shader, name), 1, (f32*)&converted);
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

intern
Gl_Shader create_basic_shader(
        String name,
        String vertex_shader,
        String fragment_shader,
        Memory_Allocator * temp_alloc,
        b32 * out_did_succeed,
        String defines = empty_string
) {
    auto error = empty_string;

    auto report_error = [&](const char* stage) {
        printf("failed to compile shader '%.*s', stage: %s. Error:\n", name.length, name.str, stage);
        printf("%.*s\n", error.length, error.str);
    };

    u32 vert_id;
    u32 frag_id;
    if(!compile_shader_part(GL_VERTEX_SHADER, vertex_shader, defines, temp_alloc, &error, &vert_id)) {
        report_error("vertex shader");
        *out_did_succeed = false;
        return {};
    }
    if(!compile_shader_part(GL_FRAGMENT_SHADER, fragment_shader, defines, temp_alloc, &error, &frag_id)) {
        report_error("fragment shader");
        *out_did_succeed = false;
        return {};
    }

    defer { glDeleteShader(vert_id); };
    defer { glDeleteShader(frag_id); };

    auto shader = make_gl_shader(name);
    glAttachShader(shader.id, vert_id);
    glAttachShader(shader.id, frag_id);

    if(!link_shader(&shader, temp_alloc, &error)) {
        glDeleteProgram(shader.id);
        report_error("linking");
        *out_did_succeed = false;
        return {};
    }

    *out_did_succeed = true;
    return shader;
}

int main(int argc, char** argv) {
    if(argc != 3) {
        printf("usage: %s <vertex shader> <fragment shader>\n", argv[0]);
        return 1;
    }

    v2 window_size = make_vector(1280, 720);
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

    u32 vao;
    u32 vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    f32 quadVerts[] = {
        -1.0f, -1.0f,
        -1.0f, 1.0f,
        1.0f, -1.0f,
        1.0f, 1.0f
    };

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    auto path_to_vertex_shader = argv[1];
    auto path_to_fragment_shader = argv[2];
    auto temp_allocator = make_arena_memory_allocator_dynamically_allocated(MEGABYTES(8));

    b32 should_quit = false;

    u64 last_frag_modify_time = 0;
    u64 last_vert_modify_time = 0;

    Gl_Shader active_shader = {};

    f64 dt = 1.0f/60.f;
    f64 time = 0;

    while(!should_quit) {
        memory_allocator_arena_reset(&temp_allocator);
        time += dt;

        if(active_shader.id) {
            set_uniform_f32(&active_shader, "time", (f32)time);
            set_uniform_v2_f32(&active_shader, "resolution", window_size);
        }

        auto startT = std::chrono::high_resolution_clock::now();

        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) {
                should_quit = true;
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

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        SDL_GL_SwapWindow(window);

        {
            static f64 time_till_load = 0;
            static b32 countdown_load = false;
            auto vert_time = plat_get_file_modification_time(path_to_vertex_shader);
            auto frag_time = plat_get_file_modification_time(path_to_fragment_shader);

            if(vert_time != last_vert_modify_time) {
                countdown_load = true;
                time_till_load = 0.05;
            }
            if(frag_time != last_frag_modify_time) {
                countdown_load = true;
                time_till_load = 0.05;
            }

            last_frag_modify_time = frag_time;
            last_vert_modify_time = vert_time;

            b32 load_program = false;

            if(countdown_load) {
                time_till_load -= dt;
                if(time_till_load <= 0) {
                    load_program = true;
                    countdown_load = false;
                }
            }

            if(load_program) {
                printf("reloading\n");

                // @EarlyOut
                while(1) {
                    auto vertex_read_result = plat_fs_read_entire_file(path_to_vertex_shader, &temp_allocator);

                    if(!vertex_read_result.did_succeed) {
                        printf("failed to read vertex shader\n");
                        break;
                    }

                    auto frag_read_result = plat_fs_read_entire_file(path_to_fragment_shader, &temp_allocator);
                    if(!frag_read_result.did_succeed) {
                        printf("failed to read frag shader\n");
                        break;
                    }

                    auto did_succeed = false;
                    auto new_program = create_basic_shader("main"_S, vertex_read_result.as_string, frag_read_result.as_string, &temp_allocator, &did_succeed);

                    if(!did_succeed) {
                        break;
                    }

                    glUseProgram(0);
                    glDeleteProgram(active_shader.id);
                    active_shader = new_program;
                    glUseProgram(active_shader.id);
                    
                    break;
                }
            }
        }

        auto tDelta = std::chrono::high_resolution_clock::now() - startT;
        dt = std::chrono::duration<f64>(tDelta).count();
    }
}
