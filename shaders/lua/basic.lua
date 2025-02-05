local frag_name = "025"

function render()
    target_fps(r, 30)
    gl_clear_color(r)
    gl_disable_alpha_blend(r)
    gl_default_viewport(r)
    gl_default_fb(r)
    gl_enable_srgb(r)

    local vert = gl_load_shader_part(r, "main quad", GL_VERTEX, "vertex/one_quad.vertex_shader")
    local frag = gl_load_shader_part(r, "main frag", GL_FRAGMENT, "fragment/" .. find_file_that_starts_with_in_folder(r, frag_name, "fragment/"))
    local shader = gl_load_shader(r, "main shader", {vert, frag})

    gl_use_shader(r, shader)
    gl_set_default_uniforms(r)
    gl_draw_quad(r)
end
