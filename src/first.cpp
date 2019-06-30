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

struct Uniform_Info {
    String name; // NOTE(justas): freeme
    GLenum type;
    u32 id;

    union {
        b32 boolean;
        f32 float32;
        s32 signed32;
        v2 vector2_f32;
        v3 vector3_f32;
        v4 vector4_f32;
    } as;
};

intern b32 show_uniform_window = false;

struct Gl_Shader_Part {
    String name = empty_string;
    u64 comparison_hash;
    u32 id = -1;
    String error;
};

struct Gl_Shader {
    String name = empty_string;
    u32 id = -1;

    Array<u64> part_hashes;
    Array<Uniform_Info> uniforms;

    String error;

    Gl_Shader() {
        part_hashes = make_array<u64>(8, &malloc_allocator, "gl shader part hashes"_S);
        uniforms = make_array<Uniform_Info>(8, &malloc_allocator, "uniforms"_S);
    }

    void clear_uniforms() {
        For(uniforms) {
            string_free(&malloc_allocator, &it->name);
        }

        array_clear(&uniforms);
    }

    void free() {
        clear_uniforms();
        array_free(&uniforms);
        array_free(&part_hashes);
        string_free(&malloc_allocator, &error);
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
void set_uniform_f32(Gl_Shader * shader, const char * name, f32 value) {
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
    glUniform3fv(get_uniform_index(shader, name), 1, (f32*)&value);
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


intern
void fetch_shader_uniform_values(Gl_Shader * shader) {
    For(shader->uniforms) {
#define UNSUPPORTED(M__WHAT) printf("unsupported" M__WHAT "\n")

        switch(it->type)
        {
            case GL_FLOAT: glGetUniformfv(shader->id, it->id, (f32*)&it->as); break;
            case GL_FLOAT_VEC2: glGetUniformfv(shader->id, it->id, (f32*)&it->as); break;
            case GL_FLOAT_VEC3: glGetUniformfv(shader->id, it->id, (f32*)&it->as); break;
            case GL_FLOAT_VEC4: glGetUniformfv(shader->id, it->id, (f32*)&it->as); break;
            case GL_INT:    glGetUniformiv(shader->id, it->id, (s32*)&it->as); break;
            case GL_INT_VEC2: 	UNSUPPORTED("ivec2"); break;
            case GL_INT_VEC3: 	UNSUPPORTED("ivec3"); break;
            case GL_INT_VEC4: 	UNSUPPORTED("ivec4"); break;
            case GL_UNSIGNED_INT: 	UNSUPPORTED("unsigned int"); break;
            case GL_UNSIGNED_INT_VEC2: 	UNSUPPORTED("uvec2"); break;
            case GL_UNSIGNED_INT_VEC3: 	UNSUPPORTED("uvec3"); break;
            case GL_UNSIGNED_INT_VEC4: 	UNSUPPORTED("uvec4"); break;
            case GL_BOOL: glGetUniformiv(shader->id, it->id, (s32*)&it->as); break;
            case GL_BOOL_VEC2: 	UNSUPPORTED("bvec2"); break;
            case GL_BOOL_VEC3: 	UNSUPPORTED("bvec3"); break;
            case GL_BOOL_VEC4: 	UNSUPPORTED("bvec4"); break;
            case GL_FLOAT_MAT2: 	UNSUPPORTED("mat2"); break;
            case GL_FLOAT_MAT3: 	UNSUPPORTED("mat3"); break;
            case GL_FLOAT_MAT4: 	UNSUPPORTED("mat4"); break;
            case GL_FLOAT_MAT2x3: 	UNSUPPORTED("mat2x3"); break;
            case GL_FLOAT_MAT2x4: 	UNSUPPORTED("mat2x4"); break;
            case GL_FLOAT_MAT3x2: 	UNSUPPORTED("mat3x2"); break;
            case GL_FLOAT_MAT3x4: 	UNSUPPORTED("mat3x4"); break;
            case GL_FLOAT_MAT4x2: 	UNSUPPORTED("mat4x2"); break;
            case GL_FLOAT_MAT4x3: 	UNSUPPORTED("mat4x3"); break;
            case GL_SAMPLER_1D: 	UNSUPPORTED("sampler1D"); break;
            case GL_SAMPLER_2D: 	UNSUPPORTED("sampler2D"); break;
            case GL_SAMPLER_3D: 	UNSUPPORTED("sampler3D"); break;
            case GL_SAMPLER_CUBE: 	UNSUPPORTED("samplerCube"); break;
            case GL_SAMPLER_1D_SHADOW: 	UNSUPPORTED("sampler1DShadow"); break;
            case GL_SAMPLER_2D_SHADOW: 	UNSUPPORTED("sampler2DShadow"); break;
            case GL_SAMPLER_1D_ARRAY: 	UNSUPPORTED("sampler1DArray"); break;
            case GL_SAMPLER_2D_ARRAY: 	UNSUPPORTED("sampler2DArray"); break;
            case GL_SAMPLER_1D_ARRAY_SHADOW: 	UNSUPPORTED("sampler1DArrayShadow"); break;
            case GL_SAMPLER_2D_ARRAY_SHADOW: 	UNSUPPORTED("sampler2DArrayShadow"); break;
            case GL_SAMPLER_2D_MULTISAMPLE: 	UNSUPPORTED("sampler2DMS"); break;
            case GL_SAMPLER_2D_MULTISAMPLE_ARRAY: 	UNSUPPORTED("sampler2DMSArray"); break;
            case GL_SAMPLER_CUBE_SHADOW: 	UNSUPPORTED("samplerCubeShadow"); break;
            case GL_SAMPLER_BUFFER: 	UNSUPPORTED("samplerBuffer"); break;
            case GL_SAMPLER_2D_RECT: 	UNSUPPORTED("sampler2DRect"); break;
            case GL_SAMPLER_2D_RECT_SHADOW: 	UNSUPPORTED("sampler2DRectShadow"); break;
            case GL_INT_SAMPLER_1D: 	UNSUPPORTED("isampler1D"); break;
            case GL_INT_SAMPLER_2D: 	UNSUPPORTED("isampler2D"); break;
            case GL_INT_SAMPLER_3D: 	UNSUPPORTED("isampler3D"); break;
            case GL_INT_SAMPLER_CUBE: 	UNSUPPORTED("isamplerCube"); break;
            case GL_INT_SAMPLER_1D_ARRAY: 	UNSUPPORTED("isampler1DArray"); break;
            case GL_INT_SAMPLER_2D_ARRAY: 	UNSUPPORTED("isampler2DArray"); break;
            case GL_INT_SAMPLER_2D_MULTISAMPLE: 	UNSUPPORTED("isampler2DMS"); break;
            case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: 	UNSUPPORTED("isampler2DMSArray"); break;
            case GL_INT_SAMPLER_BUFFER: 	UNSUPPORTED("isamplerBuffer"); break;
            case GL_INT_SAMPLER_2D_RECT: 	UNSUPPORTED("isampler2DRect"); break;
            case GL_UNSIGNED_INT_SAMPLER_1D: 	UNSUPPORTED("usampler1D"); break;
            case GL_UNSIGNED_INT_SAMPLER_2D: 	UNSUPPORTED("usampler2D"); break;
            case GL_UNSIGNED_INT_SAMPLER_3D: 	UNSUPPORTED("usampler3D"); break;
            case GL_UNSIGNED_INT_SAMPLER_CUBE: 	UNSUPPORTED("usamplerCube"); break;
            case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY: 	UNSUPPORTED("usampler2DArray"); break;
            case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY: 	UNSUPPORTED("usampler2DArray"); break;
            case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE: 	UNSUPPORTED("usampler2DMS"); break;
            case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: 	UNSUPPORTED("usampler2DMSArray"); break;
            case GL_UNSIGNED_INT_SAMPLER_BUFFER: 	UNSUPPORTED("usamplerBuffer"); break;
            case GL_UNSIGNED_INT_SAMPLER_2D_RECT: 	UNSUPPORTED("usampler2DRect"); break;
            default: UNSUPPORTED("unknown type"); break;
        }
#undef UNSUPPORTED
    }
}

intern
void flush_shader_uniform_values(Gl_Shader * shader) {
    For(shader->uniforms) {
#define UNSUPPORTED(M__WHAT) printf("unsupported" M__WHAT "\n")

        switch(it->type)
        {
            case GL_FLOAT: set_uniform_f32(shader, it->name.str, it->as.float32); break;
            case GL_FLOAT_VEC2: set_uniform_v2_f32(shader, it->name.str, it->as.vector2_f32); break;
            case GL_FLOAT_VEC3: set_uniform_v3_f32(shader, it->name.str, it->as.vector3_f32); break;
            case GL_FLOAT_VEC4: set_uniform_v4_f32(shader, it->name.str, it->as.vector4_f32); break;
            case GL_INT:    set_uniform_s32(shader, it->name.str, it->as.signed32); break;
            case GL_INT_VEC2: 	UNSUPPORTED("ivec2"); break;
            case GL_INT_VEC3: 	UNSUPPORTED("ivec3"); break;
            case GL_INT_VEC4: 	UNSUPPORTED("ivec4"); break;
            case GL_UNSIGNED_INT: 	UNSUPPORTED("unsigned int"); break;
            case GL_UNSIGNED_INT_VEC2: 	UNSUPPORTED("uvec2"); break;
            case GL_UNSIGNED_INT_VEC3: 	UNSUPPORTED("uvec3"); break;
            case GL_UNSIGNED_INT_VEC4: 	UNSUPPORTED("uvec4"); break;
            case GL_BOOL: set_uniform_s32(shader, it->name.str, it->as.boolean); break;
            case GL_BOOL_VEC2: 	UNSUPPORTED("bvec2"); break;
            case GL_BOOL_VEC3: 	UNSUPPORTED("bvec3"); break;
            case GL_BOOL_VEC4: 	UNSUPPORTED("bvec4"); break;
            case GL_FLOAT_MAT2: 	UNSUPPORTED("mat2"); break;
            case GL_FLOAT_MAT3: 	UNSUPPORTED("mat3"); break;
            case GL_FLOAT_MAT4: 	UNSUPPORTED("mat4"); break;
            case GL_FLOAT_MAT2x3: 	UNSUPPORTED("mat2x3"); break;
            case GL_FLOAT_MAT2x4: 	UNSUPPORTED("mat2x4"); break;
            case GL_FLOAT_MAT3x2: 	UNSUPPORTED("mat3x2"); break;
            case GL_FLOAT_MAT3x4: 	UNSUPPORTED("mat3x4"); break;
            case GL_FLOAT_MAT4x2: 	UNSUPPORTED("mat4x2"); break;
            case GL_FLOAT_MAT4x3: 	UNSUPPORTED("mat4x3"); break;
            case GL_SAMPLER_1D: 	UNSUPPORTED("sampler1D"); break;
            case GL_SAMPLER_2D: 	UNSUPPORTED("sampler2D"); break;
            case GL_SAMPLER_3D: 	UNSUPPORTED("sampler3D"); break;
            case GL_SAMPLER_CUBE: 	UNSUPPORTED("samplerCube"); break;
            case GL_SAMPLER_1D_SHADOW: 	UNSUPPORTED("sampler1DShadow"); break;
            case GL_SAMPLER_2D_SHADOW: 	UNSUPPORTED("sampler2DShadow"); break;
            case GL_SAMPLER_1D_ARRAY: 	UNSUPPORTED("sampler1DArray"); break;
            case GL_SAMPLER_2D_ARRAY: 	UNSUPPORTED("sampler2DArray"); break;
            case GL_SAMPLER_1D_ARRAY_SHADOW: 	UNSUPPORTED("sampler1DArrayShadow"); break;
            case GL_SAMPLER_2D_ARRAY_SHADOW: 	UNSUPPORTED("sampler2DArrayShadow"); break;
            case GL_SAMPLER_2D_MULTISAMPLE: 	UNSUPPORTED("sampler2DMS"); break;
            case GL_SAMPLER_2D_MULTISAMPLE_ARRAY: 	UNSUPPORTED("sampler2DMSArray"); break;
            case GL_SAMPLER_CUBE_SHADOW: 	UNSUPPORTED("samplerCubeShadow"); break;
            case GL_SAMPLER_BUFFER: 	UNSUPPORTED("samplerBuffer"); break;
            case GL_SAMPLER_2D_RECT: 	UNSUPPORTED("sampler2DRect"); break;
            case GL_SAMPLER_2D_RECT_SHADOW: 	UNSUPPORTED("sampler2DRectShadow"); break;
            case GL_INT_SAMPLER_1D: 	UNSUPPORTED("isampler1D"); break;
            case GL_INT_SAMPLER_2D: 	UNSUPPORTED("isampler2D"); break;
            case GL_INT_SAMPLER_3D: 	UNSUPPORTED("isampler3D"); break;
            case GL_INT_SAMPLER_CUBE: 	UNSUPPORTED("isamplerCube"); break;
            case GL_INT_SAMPLER_1D_ARRAY: 	UNSUPPORTED("isampler1DArray"); break;
            case GL_INT_SAMPLER_2D_ARRAY: 	UNSUPPORTED("isampler2DArray"); break;
            case GL_INT_SAMPLER_2D_MULTISAMPLE: 	UNSUPPORTED("isampler2DMS"); break;
            case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: 	UNSUPPORTED("isampler2DMSArray"); break;
            case GL_INT_SAMPLER_BUFFER: 	UNSUPPORTED("isamplerBuffer"); break;
            case GL_INT_SAMPLER_2D_RECT: 	UNSUPPORTED("isampler2DRect"); break;
            case GL_UNSIGNED_INT_SAMPLER_1D: 	UNSUPPORTED("usampler1D"); break;
            case GL_UNSIGNED_INT_SAMPLER_2D: 	UNSUPPORTED("usampler2D"); break;
            case GL_UNSIGNED_INT_SAMPLER_3D: 	UNSUPPORTED("usampler3D"); break;
            case GL_UNSIGNED_INT_SAMPLER_CUBE: 	UNSUPPORTED("usamplerCube"); break;
            case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY: 	UNSUPPORTED("usampler2DArray"); break;
            case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY: 	UNSUPPORTED("usampler2DArray"); break;
            case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE: 	UNSUPPORTED("usampler2DMS"); break;
            case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: 	UNSUPPORTED("usampler2DMSArray"); break;
            case GL_UNSIGNED_INT_SAMPLER_BUFFER: 	UNSUPPORTED("usamplerBuffer"); break;
            case GL_UNSIGNED_INT_SAMPLER_2D_RECT: 	UNSUPPORTED("usampler2DRect"); break;
            default: UNSUPPORTED("unknown type"); break;
        }
#undef UNSUPPORTED
    }

}

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
        part->name = name;

        auto load = false;

        if(does_asset_need_loading(asset)) {
            load = true;
        }

        if(load) {
            if(part->id != -1) {
                glDeleteProgram(part->id);
                part->id = -1;
            }

            part->comparison_hash = hash * 13 + asset->last_load_time;

            string_free(&malloc_allocator, &part->error);

            auto read = plat_fs_read_entire_file(dir, r->temp_alloc);

            if(!read.did_succeed) {
                part->error = "failed to read shader"_S;
                printf("failed to read shader %s\n", dir);
                return (void*)hash;
            }

            String error;
            u32 part_id;
            auto result = compile_shader_part(type, read.as_string, empty_string, r->temp_alloc, &error, &part_id);
            if(!result) {
                printf("failed to compile shader '%s': %.*s\n", cname, error.length, error.str);

                auto a = make_string_copy(error, &malloc_allocator);
                part->error = a.string;

                return (void*)hash;
            }

            printf("compiled new shader '%s' %d %d %llu\n", cname, type, part_id, hash);

            part->id = part_id;
        }

        return (void*)hash;
    };

    lua["find_file_that_starts_with_in_folder"] = [](Lua_Renderer * r, const char * cstarts_with, const char * in_folder) {
        auto * temp = r->temp_alloc;
        auto files = get_all_files_in_directory(in_folder, temp);

        auto starts_with = make_string(cstarts_with);

        For(files) {
            if(!it->is_file) {
                continue;
            }
            if(string_starts_with(it->name, starts_with)) {
                return temp_cstring(it->name, temp);
            }
        }

        return "file_not_found";
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
        shader->name = name;

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
            string_free(&malloc_allocator, &shader->error);

            shader->clear_uniforms();
    
            auto id = glCreateProgram();

            printf("reloading shader %s\n", cname);

            if(shader->id != -1) {
                glDeleteProgram(shader->id);
                shader->id = -1;
            }

            for(auto & kvp : t) {
                auto part_hash = (u64)kvp.second.as<void*>();
                auto * part = table_insert_or_initialize_new(&r->shader_parts, part_hash);
                *array_append(&shader->part_hashes) = part->comparison_hash;

                if(part->id == -1) {
                    printf("gl_load_shader was passed an uninitialized shader %llu!\n", part_hash);

                    auto a = make_string_copy(part->error, &malloc_allocator);
                    shader->error = a.string;

                    glDeleteProgram(shader->id);
                    return (void*)hash;
                }

                glAttachShader(id, part->id);
            }
            glLinkProgram(id);

            String error;
            if(!did_shader_compile_properly(id, &temp_allocator, &error)) {
                printf("Failed to compile shader '%s'. Error:\n", cname);
                printf("%.*s\n", error.length, error.str);

                auto a = make_string_copy(error, &malloc_allocator);
                shader->error = a.string;

                glDeleteProgram(id);
                return (void*)hash;
            }

            shader->id = id;


            {
                s32 num_uniforms;
                s32 max_name_length;
                glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &num_uniforms);
                glGetProgramiv(id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_length);

                array_reserve(&shader->uniforms, num_uniforms);

                ForRange(index, 0, num_uniforms) {
                    auto * uniform = array_append(&shader->uniforms);
                    auto name_alloc = m_new(&malloc_allocator, max_name_length);
                    uniform->id = index;
                    uniform->name = make_string((const char*)name_alloc.data);

                    s32 temp;
                    glGetActiveUniform(id, index, max_name_length, &temp, &temp, &uniform->type, (GLchar*)uniform->name.str);
                }
            }
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

    lua["gl_enable_srgb"] = [](Lua_Renderer * r) {
        glEnable(GL_FRAMEBUFFER_SRGB);
    };

    lua["gl_disable_srgb"] = [](Lua_Renderer * r) {
        glDisable(GL_FRAMEBUFFER_SRGB);
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
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    auto gl_context = SDL_GL_CreateContext(window);

    gl3wInit();
    
    glClearColor(0,0,0,1);

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
                    case SDLK_F1: {
                        show_uniform_window = !show_uniform_window;
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

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
    
        if(renderer.can_render) {
            auto & lua = renderer.lua;

            lua["dt"] = dt;
            lua["r"] = &renderer;
            lua["_time"] = shader_time;
            lua["_resolution_x"] = (f32)window_size.x;
            lua["_resolution_y"] = (f32)window_size.y;
            
            {
                sol::protected_function render_fx = lua["render"];
                auto result = render_fx();

                if(!result.valid()) {
                    sol::error e = result;
                    printf("rendering failed: %s\n", e.what());
                    renderer.can_render = false;
                }
            }
        }

        {
            struct Err {
                String source;
                String error;
            };

            auto errors = make_array<Err>(0, renderer.temp_alloc, "errors"_S);

            for(auto * kvp : renderer.shaders) {
                auto * shader = &kvp->value;

                if(shader->error.length > 0) {
                    auto * err = array_append(&errors);
                    err->error = shader->error;
                    err->source = shader->name;
                }
            }

            if(errors.watermark > 0) {
                if(ImGui::Begin("Errors")) {
                    for(auto * err : errors) {
                        ImGui::Text("====%.*s====", err->source.length, err->source.str);
                        ImGui::Text("%.*s", err->error.length, err->error.str);
                    }
                }
                ImGui::End();
            }
        }

        {
            auto uniforms = make_array<Uniform_Info*>(0, renderer.temp_alloc, "uniforms"_S);
            for(auto * kvp : renderer.shaders) {
                auto * shader = &kvp->value;

                if(shader->uniforms.watermark > 0) {
                    fetch_shader_uniform_values(shader);

                    for(auto * uniform : shader->uniforms) {
                        *array_append(&uniforms) = uniform;
                    }
                }
            }

            if(uniforms.watermark > 0 && show_uniform_window) {
                if(ImGui::Begin("Uniforms")) {


                    for(auto ** uniform_ptr : uniforms) {
                        auto * it = *uniform_ptr;

                        if(it->type == GL_FLOAT) {
                            ImGui::DragFloat(it->name.str, &it->as.float32);
                        }
                        else if(it->type == GL_FLOAT_VEC2) {
                            ImGui::DragFloat2(it->name.str, &it->as.float32);
                        }
                        else if(it->type == GL_FLOAT_VEC3) {
                            ImGui::DragFloat3(it->name.str, &it->as.float32);
                        }
                        else if(it->type == GL_FLOAT_VEC4) {
                            ImGui::DragFloat4(it->name.str, &it->as.float32);
                        }
                        else if(it->type == GL_INT) {
                            ImGui::DragInt(it->name.str, &it->as.signed32);
                        }
                        else if(it->type == GL_BOOL) {
                            ImGui::Checkbox(it->name.str, &it->as.boolean);
                        }
                    }
                }
                ImGui::End();
            }

            for(auto * kvp : renderer.shaders) {
                auto * shader = &kvp->value;

                if(shader->uniforms.watermark > 0) {
                    flush_shader_uniform_values(shader);
                }
            }
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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
