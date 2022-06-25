project('webkit-local-deps', 'c',
        version : '0.1.0')

foreach project_name: get_option('subprojects').split(',')
  if project_name != ''
    message ('Adding local subproject ' + project_name)
    subproj = subproject(project_name, required: true)
  endif
endforeach
