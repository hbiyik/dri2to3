project('dri2to3', 'c',
  version : '0.1',
  default_options : ['warning_level=2'])

cc = meson.get_compiler('c')

add_project_arguments('-Wno-unused-parameter', language : ['c'])

dl = cc.find_library('dl', required : false)
xcb = dependency('xcb')
xcb_present = dependency('xcb-present')
xcb_dri2 = dependency('xcb-dri2').partial_dependency(compile_args : true, includes : true)
xcb_dri3 = dependency('xcb-dri3')
libdrm = dependency('libdrm').partial_dependency(compile_args : true, includes : true)

shared_library('dri2to3',
               'dri2to3.c',
               dependencies : [dl, xcb, xcb_present, xcb_dri2, xcb_dri3, libdrm],
               install : true)
