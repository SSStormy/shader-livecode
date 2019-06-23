function render_shader()
end

function render()
    target_fps(r, 144)
    gl_clear_color(r)
    gl_disable_alpha_blend(r)
    gl_default_viewport(r)
    gl_default_fb(r)

    local vert = gl_load_shader_part(r, "main quad", GL_VERTEX, "vertex/one_quad.vertex_shader")
    local frag = gl_load_shader_part(r, "main frag", GL_FRAGMENT, "fragment/009_sdf.fragment_shader")
    local shader = gl_load_shader(r, "main shader", {vert, frag})

    gl_use_shader(r, shader)
    gl_set_default_uniforms(r)
    gl_draw_quad(r)
end
