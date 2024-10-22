{
  'target_defaults': {
    'dependencies': [
      "<!(node -p \"require('node-addon-api').targets\"):node_addon_api_except",
    ],
    'conditions': [
      ['OS=="win"', {
        'msvs_settings': {
            'VCMIDLTool': {
              'OutputDirectory': '<(INTERMEDIATE_DIR)',
              'HeaderFileName': '<(RULE_INPUT_ROOT).h',
            },
          },
      }],
    ],
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'terminal-handoff',
          'sources' : [
            'src/win/terminal-handoff.cc',
            'src/win/ITerminalHandoff.idl'
          ],
          'include_dirs' : [
            # To allow including "ITerminalHandoff.h"
            '<(INTERMEDIATE_DIR)',
          ],
          'libraries': [
          ],
        }
      ]
    }]
  ]
}
