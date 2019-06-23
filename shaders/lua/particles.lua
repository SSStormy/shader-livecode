--[[
function render_fb_to_screen()
    local vert = gl_load_shader_part(r, "fb to screen vert", GL_VERTEX, "vertex/one_quad.vertex_shader")
    local frag = gl_load_shader_part(r, "fb to screen frag", GL_FRAGMENT, "fragment/7_texture_copy.fragment_shader")
    local shader = gl_load_shader(r, "fb to screen shader", {vert, frag})

    gl_use_shader(r, shader)
    gl_set_default_uniforms(r)
    gl_draw_quad(r)
end
--]]

function render_background() 
    local vert = gl_load_shader_part(r, "alpha checker quad", GL_VERTEX, "vertex/one_quad.vertex_shader")
    local frag = gl_load_shader_part(r, "alpha checker frag", GL_FRAGMENT, "fragment/6_alpha_checkers.fragment_shader")
    local shader = gl_load_shader(r, "alpha checkers shader", {vert, frag})

    gl_use_shader(r, shader)
    gl_set_default_uniforms(r)
    gl_draw_quad(r)
end

function render_shader()
    local vert = gl_load_shader_part(r, "main quad", GL_VERTEX, "vertex/one_quad.vertex_shader")
    local frag = gl_load_shader_part(r, "main frag", GL_FRAGMENT, "fragment/5_particles.fragment_shader")
    local shader = gl_load_shader(r, "main shader", {vert, frag})

    gl_use_shader(r, shader)
    gl_set_default_uniforms(r)
    gl_draw_quad(r)
end

function render()
    target_fps(r, 144)
    gl_clear_color(r)
    gl_enable_alpha_blend(r)
    gl_default_viewport(r)
    gl_default_fb(r)

    --[[
    local fb = gl_create_fb(r, "deferred fb", {
        color = "rgba"
    });

    gl_use_fb(r, fb)
    --]]


    render_background()
    render_shader()

end
