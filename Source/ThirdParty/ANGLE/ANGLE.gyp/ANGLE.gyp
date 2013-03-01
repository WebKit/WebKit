{
  'includes': {
    '../ANGLE.gypi',
  },

  'variables': {
    'ANGLE': '..',
    'FLEX%': 'flex',
    'BISON%': 'bison',
  },

  'targets': [
    {
      'target_name': 'angle',
      'type': 'static_library',
      'sources': [ '<@(angle_files)', ],
      'include_dirs': [ '<@(angle_include_dirs)' ],
      'direct_dependent_settings': {
        'include_dirs': [ '<@(angle_include_dirs)' ],
      },
      'actions': [
        {
          'action_name': 'GenerateGLShaderLanguageLexer',
          'inputs': [ '<(ANGLE)/src/compiler/glslang.l' ],
          'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/ANGLE/glslang.cpp', ],
          'action': [
            '<(FLEX)',
            '--noline',
            '--nounistd',
            '--outfile=<(SHARED_INTERMEDIATE_DIR)/ANGLE/glslang.cpp',
            '<(_inputs)',
          ],
          'message': 'Generating OpenGL shader language lexer',
        },
        {
          'action_name': 'GenerateGLShaderLanguageParser',
          'inputs': [ '<(ANGLE)/src/compiler/glslang.y' ],
          'outputs': [ 
            '<(SHARED_INTERMEDIATE_DIR)/ANGLE/glslang_tab.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/ANGLE/glslang_tab.h',
          ],
          'action': [
            '<(BISON)',
            '--no-lines',
            '--defines=<(SHARED_INTERMEDIATE_DIR)/ANGLE/glslang_tab.h',
            '--skeleton=yacc.c',
            '--output=<(SHARED_INTERMEDIATE_DIR)/ANGLE/glslang_tab.cpp',
            '<(_inputs)',
          ],
          'message': 'Generating OpenGL shader language parser',
        },
      ]
    },
  ],
}
