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
};

struct Asset_Entry {
    const char * path = 0;
    b32 needs_first_load = true;
    u64 last_load_time = 0;
    f64 countdown_to_load = 0;
};

intern auto asset_catalogue = make_table<Asset_Entry>(8, &malloc_allocator, "asset catalogue"_S);

intern force_inline
Asset_Entry * get_asset(u64 hash, const char * path) {
    auto * asset = table_insert_or_initialize_new(&asset_catalogue, hash);
    asset->path = path;
    return asset;
}

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

intern auto shader_parts = make_table<Gl_Shader_Part>(8, &malloc_allocator, "shader parts"_S);
intern auto shaders = make_table<Gl_Shader>(8, &malloc_allocator, "shaders"_S);

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

struct Uniform_Info {
    Memory_Allocation name_alloc;

    const char * name;
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

intern Gl_Shader active_shader;

intern auto shader_uniforms = make_array<Uniform_Info>(0, &malloc_allocator, "shader uniforms"_S);

intern
void fetch_shader_uniform_values() {
    For(shader_uniforms) {
#define UNSUPPORTED(M__WHAT) printf("unsupported" M__WHAT "\n")

        switch(it->type)
        {
            case GL_FLOAT: glGetUniformfv(active_shader.id, it->id, (f32*)&it->as); break;
            case GL_FLOAT_VEC2: glGetUniformfv(active_shader.id, it->id, (f32*)&it->as); break;
            case GL_FLOAT_VEC3: glGetUniformfv(active_shader.id, it->id, (f32*)&it->as); break;
            case GL_FLOAT_VEC4: glGetUniformfv(active_shader.id, it->id, (f32*)&it->as); break;
            case GL_INT:    glGetUniformiv(active_shader.id, it->id, (s32*)&it->as); break;
            case GL_INT_VEC2: 	UNSUPPORTED("ivec2"); break;
            case GL_INT_VEC3: 	UNSUPPORTED("ivec3"); break;
            case GL_INT_VEC4: 	UNSUPPORTED("ivec4"); break;
            case GL_UNSIGNED_INT: 	UNSUPPORTED("unsigned int"); break;
            case GL_UNSIGNED_INT_VEC2: 	UNSUPPORTED("uvec2"); break;
            case GL_UNSIGNED_INT_VEC3: 	UNSUPPORTED("uvec3"); break;
            case GL_UNSIGNED_INT_VEC4: 	UNSUPPORTED("uvec4"); break;
            case GL_BOOL: glGetUniformiv(active_shader.id, it->id, (s32*)&it->as); break;
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
void flush_shader_uniform_values() {
    For(shader_uniforms) {
#define UNSUPPORTED(M__WHAT) printf("unsupported" M__WHAT "\n")

        switch(it->type)
        {
            case GL_FLOAT: set_uniform_f32(&active_shader, it->name, it->as.float32); break;
            case GL_FLOAT_VEC2: set_uniform_v2_f32(&active_shader, it->name, it->as.vector2_f32); break;
            case GL_FLOAT_VEC3: set_uniform_v3_f32(&active_shader, it->name, it->as.vector3_f32); break;
            case GL_FLOAT_VEC4: set_uniform_v4_f32(&active_shader, it->name, it->as.vector4_f32); break;
            case GL_INT:    set_uniform_s32(&active_shader, it->name, it->as.signed32); break;
            case GL_INT_VEC2: 	UNSUPPORTED("ivec2"); break;
            case GL_INT_VEC3: 	UNSUPPORTED("ivec3"); break;
            case GL_INT_VEC4: 	UNSUPPORTED("ivec4"); break;
            case GL_UNSIGNED_INT: 	UNSUPPORTED("unsigned int"); break;
            case GL_UNSIGNED_INT_VEC2: 	UNSUPPORTED("uvec2"); break;
            case GL_UNSIGNED_INT_VEC3: 	UNSUPPORTED("uvec3"); break;
            case GL_UNSIGNED_INT_VEC4: 	UNSUPPORTED("uvec4"); break;
            case GL_BOOL: set_uniform_s32(&active_shader, it->name, it->as.boolean); break;
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

intern u32 quad_vao;
intern u32 quad_vbo;

intern
void repopulate_shader_uniform_array() {
    For(shader_uniforms) {
        m_free(&malloc_allocator, it->name_alloc);
    }
    array_clear(&shader_uniforms);

    s32 num_uniforms;
    s32 max_name_length;
    glGetProgramiv(active_shader.id, GL_ACTIVE_UNIFORMS, &num_uniforms);
    glGetProgramiv(active_shader.id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_length);

    array_reserve(&shader_uniforms, num_uniforms);

    ForRange(index, 0, num_uniforms) {
        auto * uniform = array_append(&shader_uniforms);
        auto name_alloc = m_new(&malloc_allocator, max_name_length);
        uniform->id = index;
        uniform->name = (const char*)name_alloc.data;
        uniform->name_alloc = name_alloc;

        s32 temp;
        glGetActiveUniform(active_shader.id, index, max_name_length, &temp, &temp, &uniform->type, (GLchar*)uniform->name);
    }
}

intern auto temp_allocator = make_arena_memory_allocator(m_new(&malloc_allocator, MEGABYTES(16)));

intern 
b32 try_load_render_lua_script(
        const char * script_dir,
        sol::state & out_state,
        String * out_error
        
) {
    auto script_read = plat_fs_read_entire_file(script_dir, &temp_allocator);
    if(!script_read.did_succeed) {
        *out_error = "failed to read lua script\n"_S;
        return false;
    }

    auto cstring_script = temp_cstring(script_read.as_string, &temp_allocator);

    auto script_result = out_state.script(cstring_script, [](lua_State*, sol::protected_function_result pfr) {
        return pfr;

    }, "render script");

    if(!script_result.valid()) {
        sol::error e = script_result;
        *out_error = make_string(e.what());
        return false;
    }

    out_state.open_libraries(sol::lib::base);
    out_state["GL_VERTEX"] = GL_VERTEX_SHADER;
    out_state["GL_FRAGMENT"] = GL_FRAGMENT_SHADER;

    out_state["gl_load_shader_part"] = [&](const char * cname, s32 type, const char * dir) {
        auto name = make_string(cname);
        auto hash = hash_fnv(name);

        auto * asset = get_asset(hash, dir);
        auto * part = table_insert_or_initialize_new(&shader_parts, hash);

        auto load = false;
        if(does_asset_need_loading(asset)) {
            load = true;
        }
        else if(!string_equals_case_sensitive(name, part->name)) {
            load = true;
        }

        if(load) {
            auto read = plat_fs_read_entire_file(dir, &temp_allocator);

            if(!read.did_succeed) {
                printf("failed to read shader %s\n", dir);
                return (void*)0;
            }

            String error;
            u32 part_id;
            auto result = compile_shader_part(type, read.as_string, empty_string, &temp_allocator, &error, &part_id);
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

    out_state["gl_use_shader"] = [&](void * void_shader_hash) {
        auto shader_hash = (u64)void_shader_hash;
        auto * shader = table_insert_or_initialize_new(&shaders, shader_hash);
        if(shader->id == -1) {
            return;
        }

        glUseProgram(shader->id);
        active_shader = *shader;

        repopulate_shader_uniform_array();
    };

    out_state["gl_load_shader"] = [&](const char * cname, sol::table t) {

        auto name = make_string(cname);
        auto hash = hash_fnv(name);

        auto * shader = table_insert_or_initialize_new(&shaders, hash);

        auto needs_reload = false;

        auto has_hash_mismatch = false;
        auto num_parts = 0;
        for(auto & kvp : t) {
            num_parts++;

            auto part_hash = (u64)kvp.second.as<void*>();
            auto * part = table_insert_or_initialize_new(&shader_parts, part_hash);

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
                auto * part = table_insert_or_initialize_new(&shader_parts, part_hash);
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

    out_state["gl_default_fb"] = [&]() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    };

    out_state["gl_draw_quad"] = [&]() {
        glBindVertexArray(quad_vao);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    };

    out_state["gl_uniform_f32"] = [&](const char * uniform, f32 num) {
        set_uniform_f32(&active_shader, uniform, num);
    };

    out_state["gl_uniform_v2_f32"] = [&](const char * uniform, f32 x, f32 y) {
        v2 v = {x,y};
        set_uniform_v2_f32(&active_shader, uniform, v);
    };

    return true;
}

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("usage: %s <loop lua file> \n", argv[0]);
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
    f64 time = 0;

    auto script_file_dir = argv[1];
    auto script_asset_hash = hash_fnv(make_string(script_file_dir));

    {
        
        auto * script_asset = get_asset(script_asset_hash, script_file_dir);
        script_asset->path = script_file_dir;
    }

    sol::state lua_state; 
    auto did_load_lua = false;

    while(!should_quit) {
        auto startT = std::chrono::high_resolution_clock::now();

        memory_allocator_arena_reset(&temp_allocator);
        time += dt;

        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if(event.type == SDL_QUIT) {
                should_quit = true;
            }
            else if(event.type == SDL_KEYDOWN) {
                switch(event.key.keysym.sym) {
                    case SDLK_F1: {
                        show_uniform_window = !show_uniform_window;
                        break;
                    }
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

        auto * script_asset = get_asset(script_asset_hash, script_file_dir);

        if(does_asset_need_loading(script_asset)) {
            String error;

            sol::state new_lua_state;
            if(!try_load_render_lua_script(script_asset->path, new_lua_state, &error)) {
                printf("lua error: %.*s\n", error.length, error.str);
                did_load_lua = false;
            }
            else {
                lua_state = std::move(new_lua_state);
                did_load_lua = true;
                printf("reloaded lua\n");
            }
        }

        if(did_load_lua) {
            sol::protected_function render_fx = lua_state["render"];

            lua_state["_time"] = time;
            lua_state["_resolution_x"] = (f32)window_size.x;
            lua_state["_resolution_y"] = (f32)window_size.y;

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame(window);
            ImGui::NewFrame();

            auto result = render_fx(dt);
            if(!result.valid()) {
                sol::error e = result;
                printf("rendering failed: %s\n", e.what());
            }

            if(show_uniform_window && ImGui::Begin("Uniforms")) {
                fetch_shader_uniform_values();

                For(shader_uniforms) {
                    if(it->type == GL_FLOAT) {
                        ImGui::DragFloat(it->name, &it->as.float32);
                    }
                    else if(it->type == GL_FLOAT_VEC2) {
                        ImGui::DragFloat2(it->name, &it->as.float32);
                    }
                    else if(it->type == GL_FLOAT_VEC3) {
                        ImGui::DragFloat3(it->name, &it->as.float32);
                    }
                    else if(it->type == GL_FLOAT_VEC4) {
                        ImGui::DragFloat4(it->name, &it->as.float32);
                    }
                    else if(it->type == GL_INT) {
                        ImGui::DragInt(it->name, &it->as.signed32);
                    }
                    else if(it->type == GL_BOOL) {
                        ImGui::Checkbox(it->name, &it->as.boolean);
                    }
                }

                flush_shader_uniform_values();
                ImGui::End();
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        SDL_GL_SwapWindow(window);

        auto tDelta = std::chrono::high_resolution_clock::now() - startT;
        dt = std::chrono::duration<f64>(tDelta).count();
    }
}
