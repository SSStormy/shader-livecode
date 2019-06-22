function render(dt)
    local vert = gl_load_shader_part("main quad", GL_VERTEX, "vertex/one_quad.vertex_shader")
    local frag = gl_load_shader_part("main frag", GL_FRAGMENT, "fragment/fbm_mirror.fragment_shader")

    local shader = gl_load_shader("main shader", {vert, frag})

    gl_default_fb()
    gl_use_shader(shader)
    gl_uniform_f32("time", _time)
    gl_uniform_v2_f32("resolution", _resolution_x, _resolution_y)

    gl_draw_quad()
end
